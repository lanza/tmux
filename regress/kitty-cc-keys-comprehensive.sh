#!/bin/sh

# Test: kitty keyboard protocol in tmux -CC control mode
#
# =========================================================================
# WHAT THIS TESTS
# =========================================================================
#
# Two independent bugs in tmux -CC kitty keyboard handling:
#
# Bug A (%output filter): tmux strips kitty push/pop/set sequences from
#   %output sent to control clients.  This prevents iTerm2 from entering
#   kitty mode, so iTerm2 sends legacy-encoded keys instead of kitty CSI u.
#
# Bug B (double encoding): When iTerm2 IS in kitty mode and sends kitty
#   CSI u sequences via the gateway (hex bytes split across send commands),
#   tmux re-encodes each byte through input_key_kitty() instead of passing
#   the raw bytes through to the pane.  The ESC byte (0x1b) becomes CSI 27u,
#   destroying the original sequence.
#
# =========================================================================
# CORRECT BEHAVIOR (both bugs fixed)
# =========================================================================
#
#   1. App pushes kitty mode (CSI >31u)
#   2. tmux forwards CSI >31u in %output to iTerm2
#   3. iTerm2 enters kitty mode, sends kitty CSI u for keystrokes
#   4. Gateway sends: send -t %N 0x1b 0x5b; send -lt %N 97; ...
#   5. tmux passes raw bytes through to pane: \033[97;5u
#   6. App receives correct kitty encoding
#
# =========================================================================
# HOW TO VERIFY MANUALLY
# =========================================================================
#
#   In tmux -CC (via iTerm2):
#     printf "\033[>31u"; stty raw -echo; cat -v
#   Then press keys and see what the pane receives.
#   Or: kitten show-key -m kitty
#

PATH=/bin:/usr/bin
TERM=screen

TESTDIR=$(cd -- "$(dirname "$0")" && pwd)
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f "$TESTDIR/../tmux")
TMUX="$TEST_TMUX -Ltest-kittyccprod"
$TMUX kill-server 2>/dev/null
sleep 0.5

RC=0
TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"; $TMUX kill-server 2>/dev/null' EXIT

RED=$(printf '\033[31m')
GREEN=$(printf '\033[32m')
YELLOW=$(printf '\033[33m')
RESET=$(printf '\033[0m')

# Decode cat -v output into human-readable key descriptions.
# ^[[97;5u       -> "CSI 97 ; 5 u"
# ^[[27u^[[97u   -> "CSI 27 u | CSI 97 u"  (two separate key events)
decode_catv () {
	echo "$1" | sed \
		-e 's/\^\[\[/\x00CSI /g' \
		-e 's/;/ ; /g' \
		-e 's/\([0-9]\)\([A-Su~]\)/\1 \2/g' | \
	tr '\000' '\n' | sed '/^$/d' | \
	tr '\n' '|' | sed -e 's/|$//' -e 's/|/ | /g' -e 's/  */ /g'
}

# Tri-state check:
#   PASS          = actual matches expected (correct behavior)
#   FAIL-EXPECTED = actual matches known_broken (bug confirmed, not a surprise)
#   FAIL          = actual matches neither (unexpected behavior)
#
# $1 = label
# $2 = expected (correct behavior)
# $3 = known_broken (current broken behavior)
# $4 = actual
check3 () {
	label=$1
	expected=$2
	known_broken=$3
	actual=$4

	expected_decoded=$(decode_catv "$expected")
	actual_decoded=$(decode_catv "$actual")
	broken_decoded=$(decode_catv "$known_broken")

	if [ "$actual" = "$expected" ]; then
		printf '%s[PASS]%s %s\n' "$GREEN" "$RESET" "$label"
		printf '       %s\n' "$actual_decoded"
	elif [ "$actual" = "$known_broken" ]; then
		printf '%s[FAIL-EXPECTED]%s %s\n' "$YELLOW" "$RESET" "$label"
		printf '       want: %s\n' "$expected_decoded"
		printf '       got:  %s\n' "$actual_decoded"
		RC=1
	else
		printf '%s[FAIL]%s %s\n' "$RED" "$RESET" "$label"
		printf '       want:  %s\n' "$expected_decoded"
		printf '       known: %s\n' "$broken_decoded"
		printf '       got:   %s\n' "$actual_decoded"
		RC=1
	fi
}

$TMUX -f/dev/null new -x80 -y2 -d -s kcc || exit 1
sleep 0.3
$TMUX set -g kitty-keys always
$TMUX set -g escape-time 0

send_via_control () {
	echo "$1" | $TMUX -C attach -t kcc >/dev/null 2>&1 &
	_pid=$!
	sleep 0.3
	kill $_pid 2>/dev/null
	wait $_pid 2>/dev/null
}

# $1 = gateway command  $2 = expected  $3 = known_broken  $4 = label
run_test () {
	W=$($TMUX new-window -P -- sh -c \
		'printf "\033[>31u"; stty raw -echo && cat -tv')
	sleep 0.3

	eval "cmd=\"$1\""
	send_via_control "$cmd"
	sleep 0.2

	actual=$($TMUX capturep -pt"$W" | head -1)
	$TMUX kill-window -t"$W" 2>/dev/null
	check3 "$4" "$2" "$3" "$actual"
}

# =====================================================================
printf '\n=== Bug A: %%output filter strips kitty push ===\n\n'
# =====================================================================

(echo "refresh-client -fpause-after=1"; sleep 2) \
	| $TMUX -C attach -t kcc > "$TMPDIR/control-output.txt" 2>/dev/null &
CPID=$!
sleep 0.5
KW=$($TMUX new-window -P -- sh -c 'printf "\033[>31u"; sleep 3')
sleep 1
kill $CPID 2>/dev/null
wait $CPID 2>/dev/null
$TMUX kill-window -t"$KW" 2>/dev/null

if grep -q '>31u\|>1u' "$TMPDIR/control-output.txt"; then
	result='present'
else
	result='stripped'
fi

if [ "$result" = "present" ]; then
	printf '%s[PASS]%s %%output delivers kitty push to control client\n' "$GREEN" "$RESET"
elif [ "$result" = "stripped" ]; then
	printf '%s[FAIL-EXPECTED]%s %%output delivers kitty push to control client\n' "$YELLOW" "$RESET"
	printf '       want: present (so iTerm2 enters kitty mode)\n'
	printf '       got:  stripped (iTerm2 stays legacy)\n'
	RC=1
fi

# =====================================================================
printf '\n=== Bug B: gateway kitty bytes re-encoded instead of passed through ===\n\n'
# =====================================================================

# All modifier + a permutations
run_test \
	'send -t $W 0x1b 0x5b; send -lt $W 97; send -t $W 0x3b; send -lt $W 5u' \
	'^[[97;5u' \
	'^[[27u^[[91u^[[57u^[[55u^[[59u^[[53u^[[117u' \
	'Ctrl-a'

run_test \
	'send -t $W 0x1b 0x5b; send -lt $W 97; send -t $W 0x3b; send -lt $W 3u' \
	'^[[97;3u' \
	'^[[27u^[[91u^[[57u^[[55u^[[59u^[[51u^[[117u' \
	'Option-a (Alt)'

run_test \
	'send -t $W 0x1b 0x5b; send -lt $W 97; send -t $W 0x3b; send -lt $W 2u' \
	'^[[97;2u' \
	'^[[27u^[[91u^[[57u^[[55u^[[59u^[[50u^[[117u' \
	'Shift-a'

run_test \
	'send -t $W 0x1b 0x5b; send -lt $W 97; send -t $W 0x3b; send -lt $W 6u' \
	'^[[97;6u' \
	'^[[27u^[[91u^[[57u^[[55u^[[59u^[[54u^[[117u' \
	'Ctrl-Shift-a'

run_test \
	'send -t $W 0x1b 0x5b; send -lt $W 97; send -t $W 0x3b; send -lt $W 7u' \
	'^[[97;7u' \
	'^[[27u^[[91u^[[57u^[[55u^[[59u^[[55u^[[117u' \
	'Ctrl-Option-a'

run_test \
	'send -t $W 0x1b 0x5b; send -lt $W 97; send -t $W 0x3b; send -lt $W 4u' \
	'^[[97;4u' \
	'^[[27u^[[91u^[[57u^[[55u^[[59u^[[52u^[[117u' \
	'Option-Shift-a'

run_test \
	'send -t $W 0x1b 0x5b; send -lt $W 97; send -t $W 0x3b; send -lt $W 8u' \
	'^[[97;8u' \
	'^[[27u^[[91u^[[57u^[[55u^[[59u^[[56u^[[117u' \
	'Ctrl-Option-Shift-a'

run_test \
	'send -t $W 0x1b 0x5b; send -lt $W 97; send -t $W 0x3b; send -lt $W 9u' \
	'^[[97;9u' \
	'^[[27u^[[91u^[[57u^[[55u^[[59u^[[57u^[[117u' \
	'Cmd-a (Super)'

# Plain keys
run_test \
	'send -t $W 0x1b 0x5b; send -lt $W 97u' \
	'^[[97u' \
	'^[[27u^[[91u^[[57u^[[55u^[[117u' \
	'a (plain)'

run_test \
	'send -t $W 0x1b 0x5b; send -lt $W 27u' \
	'^[[27u' \
	'^[[27u^[[91u^[[50u^[[55u^[[117u' \
	'Escape'

# Special keys
run_test \
	'send -t $W 0x1b 0x5b; send -lt $W 1; send -t $W 0x3b; send -lt $W 1A' \
	'^[[1;1A' \
	'^[[27u^[[91u^[[49u^[[59u^[[49u^[[65u' \
	'Up arrow'

run_test \
	'send -t $W 0x1b 0x5b; send -lt $W 13u' \
	'^[[13u' \
	'^[[27u^[[91u^[[49u^[[51u^[[117u' \
	'Enter'

run_test \
	'send -t $W 0x1b 0x5b; send -lt $W 9u' \
	'^[[9u' \
	'^[[27u^[[91u^[[57u^[[117u' \
	'Tab'

run_test \
	'send -t $W 0x1b 0x5b; send -lt $W 1; send -t $W 0x3b; send -lt $W 1P' \
	'^[[1;1P' \
	'^[[27u^[[91u^[[49u^[[59u^[[49u^[[80u' \
	'F1'

# Focus event
printf '\n=== Focus event ===\n\n'
W=$($TMUX new-window -P -- sh -c \
	'printf "\033[>31u"; stty raw -echo && cat -tv')
sleep 0.3
send_via_control "send -t $W 0x1b 0x5b; send -lt $W O"
sleep 0.2
actual=$($TMUX capturep -pt"$W" | head -1)
$TMUX kill-window -t"$W" 2>/dev/null
case "$actual" in
	*'27u'*|*'91u'*|*'79u'*|*'[O'*)
		printf '%s[FAIL]%s Focus out\n' "$RED" "$RESET"
		printf '       want: (no visible text)\n'
		printf '       got:  %s\n' "$(decode_catv "$actual")"
		RC=1
		;;
	*)
		printf '%s[PASS]%s Focus out (no visible text)\n' "$GREEN" "$RESET"
		;;
esac

printf '\n'

$TMUX list-sessions >/dev/null 2>&1
if [ $? -eq 0 ]; then
	printf '%s[PASS]%s server alive\n' "$GREEN" "$RESET"
else
	printf '%s[FAIL]%s server alive\n' "$RED" "$RESET"
	RC=1
fi

$TMUX kill-server 2>/dev/null

printf '\n'
if [ $RC -eq 0 ]; then
	printf 'All tests passed.\n'
else
	printf 'Some tests FAILED.\n'
fi

exit $RC
