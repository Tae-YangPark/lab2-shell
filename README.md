# lab2-shell

교재 357~358쪽의 `simplesh.c`를 확장한 C11 리눅스 셸입니다.
외부 명령 실행, `exit`, 백그라운드 실행(`&`), 리다이렉션(`<`, `>`), 파이프(`|`)를 지원합니다.

## 빌드와 실행

Ubuntu에서 GCC와 Make가 필요합니다. 자동 테스트에는 Python 3이 필요합니다.

```sh
make
./lab2-shell
make test
```

`make test`는 Linux 가상 터미널(PTY)에서 Ctrl+C 같은 키 입력을 재현합니다. Ubuntu에서 PTY를 따로 설치할 필요는 없습니다.

`ls [-a] [--] [path ...]`, `pwd`, `cd [directory]`는 직접 구현했습니다. 단독으로 실행한 `cd`는 셸의 작업 디렉터리를 변경하며, 인자가 없으면 `HOME`으로 이동합니다.

Linux에서 Ctrl+C는 SIGINT, Ctrl+Z는 SIGTSTP, Ctrl+\는 SIGQUIT을 보냅니다. Ctrl+Z로 중지된 포그라운드 작업은 종료하고 프롬프트로 돌아갑니다. 공지의 Ctrl+Z 표기는 교수님께 확인이 필요합니다.

## 역할 분담

| 담당 | 명령 | 구현 파일 |
|---|---|---|
| 공통 셸·디렉터리 명령 담당 | `ls`, `pwd`, `cd` | `src/directory_commands.c` |
| 주재홍 | `mkdir`, `rmdir`, `ln`, `cp` | `src/mkdir.c`, `src/rmdir.c`, `src/ln.c`, `src/cp.c` (예정) |
| 하지성 | `rm`, `mv`, `cat`, `grep` | `src/rm.c`, `src/mv.c`, `src/cat.c`, `src/grep.c` (예정) |

명령 함수는 `int cmd_name(int argc, char *const argv[])` 형식으로 작성하고, 표준 입출력을 사용합니다. 성공은 0, 실행 오류는 1, 인자 오류는 2를 반환합니다. 통합할 때 `include/commands.h`에 선언을, `src/shell.c`의 `builtins` 표에 명령을, `Makefile`의 `OBJ`에 오브젝트 파일을 추가합니다.

라이선스는 [MIT](LICENSE)입니다.
