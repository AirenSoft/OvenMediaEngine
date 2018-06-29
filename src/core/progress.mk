ifndef TARGET_COUNTER
# 현재 Makefile에서 처리할 TARGET의 총 개수를 구함
# 아래 명령을 실행하게 되면, TARGET_COUNTER 변수를 make로 넣어 실행하게 되는데, 이 때 표시된 TARGET_COUNTER의 개수를 가지고 총 target 개수를 측정함
#
# -n: 실행할 명령만 보여주고, 실제로는 실행하지 않음
# -r: 암묵적으로 선언된 rule들을 선언하지 않음
# -R: 기본적으로 선언되는 변수들을 선언하지 않음
# -f: Make할 파일 지정. $(MAKEFILE_LIST)의 가장 첫 번째 단어는 자기 자신
TOTAL_TARGET_COUNT := $(shell $(MAKE) $(MAKECMDGOALS) --no-print-directory -nrRf $(firstword $(MAKEFILE_LIST)) TARGET_COUNTER="TARGET_FOUND" 2>/dev/null | grep -c "^TARGET_FOUND$$")

# TOTAL_TARGET_COUNT로 부터, 패딩을 몇 글자로 해야하는지 bc 명령을 사용하여 계산함
# 1) log(TOTAL_TARGET_COUNT)/log(10)로, 일단 몇 자리 수인지 계산 (2.12944와 같이 소수점 형태로 나옴)
# 2) 소수를 정수로 변경하기 위해, "<값>/1" 계산
PADDING_COUNT := $(shell echo "(`echo 'l($(TOTAL_TARGET_COUNT))/l(10)' | bc -l` + 1) / 1" | bc)

# 현재까지 몇 개 처리되었는지 계산하기 위한 변수. 이 안에 들어있는 *의 개수가 현재까지 처리된 개수임
PROCESSED_TARGET_LIST := *

# 마커의 개수를 세서 C에 넣고, 마커의 개수를 늘림
MARKER_COUNT = $(words $(PROCESSED_TARGET_LIST))
INCREASE_COUNT = $(eval PROCESSED_TARGET_LIST := $(PROCESSED_TARGET_LIST) *)
CURRENT_PROGRESS = "[`printf \"$(ANSI_GREEN)%$(PADDING_COUNT)d$(ANSI_RESET)/$(ANSI_GREEN)%$(PADDING_COUNT)d$(ANSI_RESET)|$(ANSI_PURPLE)%3d%%$(ANSI_RESET)\" $(MARKER_COUNT) $(TOTAL_TARGET_COUNT) \`expr \\\`expr $(MARKER_COUNT) '*' 100\\\` / $(TOTAL_TARGET_COUNT)\``$(ANSI_RESET)] "
TARGET_COUNTER :=
else
# TARGET의 총 개수를 구하는 중. 위에서 make 할 때, command line에서 전달했던 "TARGET_FOUND"가 표시됨
endif
