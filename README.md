# lab2-shell

리눅스에서 실행되는 간단한 셸을 C로 구현한 팀 프로젝트입니다. 교재 357~358쪽의 `simplesh.c`를 바탕으로 만들었습니다.
외부 명령 실행, `exit`, 백그라운드 실행(`&`), 리다이렉션(`<`, `>`), 파이프(`|`)를 지원합니다.

## 빌드와 실행

Ubuntu에서 GCC와 Make로 빌드합니다. 컴파일에는 C11 표준을 사용하며, 자동 테스트에는 Python 3이 필요합니다.

```sh
make
./lab2-shell
make test
```

`make test`는 Linux 가상 터미널(PTY)에서 Ctrl+C 같은 키 입력을 재현합니다. Ubuntu에서 PTY를 따로 설치할 필요는 없습니다.

`ls [-a] [--] [path ...]`, `pwd`, `cd [directory]`는 직접 구현했습니다. 단독으로 실행한 `cd`는 셸의 작업 디렉터리를 변경하며, 인자가 없으면 `HOME`으로 이동합니다.

Linux에서 Ctrl+C는 SIGINT, Ctrl+Z는 SIGTSTP, Ctrl+\는 SIGQUIT을 보냅니다. Ctrl+Z로 중지된 포그라운드 작업은 종료하고 프롬프트로 돌아갑니다. 공지의 Ctrl+Z 표기는 교수님께 확인이 필요합니다.

### 실행 예시

`./lab2-shell`을 실행한 뒤 아래 내용을 한 줄씩 입력합니다. 첫 줄은 최대 다섯 항목을 `listing.txt`에 저장하고, 둘째 줄은 그 파일의 첫 줄을 출력합니다. `sleep`은 백그라운드에서 실행됩니다.

```text
ls -a | head -n 5 > listing.txt
head -n 1 < listing.txt
sleep 5 &
exit
```

## 역할 분담

| 과제 번호 | 담당 | 기능·명령 | 구현 파일 |
|---|---|---|---|
| 1번 | 박태양 (팀장) | `exit`로 셸 종료 | `src/shell.c` |
| 2번 | 박태양 (팀장) | `&` 백그라운드 실행 | `src/shell.c`, `src/parser.c` |
| 3번 | 박태양 (팀장) | Ctrl+C 등 터미널 시그널 처리 | `src/shell.c` |
| 4번 | 박태양 (팀장) | `<`, `>` 리다이렉션 및 파이프(`\|`) | `src/shell.c`, `src/parser.c` |
| 5번 | 박태양 (팀장) | `ls`, `pwd`, `cd` | `src/directory_commands.c` |
| 5번 | 주재홍 | `mkdir`, `rmdir`, `ln`, `cp` | `src/mkdir.c`, `src/rmdir.c`, `src/ln.c`, `src/cp.c` (예정) |
| 5번 | 하지성 | `rm`, `mv`, `cat`, `grep` | `src/rm.c`, `src/mv.c`, `src/cat.c`, `src/grep.c` (예정) |

명령 함수는 `int cmd_name(int argc, char *const argv[])` 형식으로 작성합니다. 일반 출력은 `stdout`, 오류 메시지는 `stderr`를 사용합니다. `exit()`를 호출하는 대신 성공은 0, 실행 오류는 1, 인자 오류는 2를 반환합니다. 통합할 때 `include/commands.h`에 선언을, `src/shell.c`의 `builtins` 표에 명령을, `Makefile`의 `OBJ`에 오브젝트 파일을 추가합니다. 등록 예시는 기존 `cmd_ls`를 참고합니다.

라이선스는 [MIT](LICENSE)입니다.
