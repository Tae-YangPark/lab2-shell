import os
import pathlib
import pty
import re
import resource
import select
import signal
import subprocess
import sys
import tempfile
import time

BINARY = pathlib.Path(sys.argv[1]).resolve()
PROMPT = b"lab2-shell$ "


def batch(text, cwd, path="/usr/bin:/bin"):
    return subprocess.run(
        [BINARY], input=text, text=True, capture_output=True,
        cwd=cwd, env={**os.environ, "PATH": path}, timeout=5,
    )


def test_batch(cwd):
    sub = cwd / "a dir"
    sub.mkdir()
    (sub / "data").write_text("alpha\nbeta\n")
    (sub / ".hidden").touch()
    result = batch(
        "cd 'a dir'\npwd\nls\nls -a >listing\n"
        "/usr/bin/tr a-z A-Z <data|/usr/bin/head -n 1 >out\n"
        "/usr/bin/printf '%s\\n' 'a|b' \"\" a\\ b\n"
        "cd .. | /usr/bin/true\npwd\nexit\npwd\n",
        cwd, "/nonexistent",
    )
    assert result.returncode == 0 and not result.stderr, result
    assert result.stdout.splitlines() == [str(sub), "data", "a|b", "", "a b", str(sub)], result
    assert (sub / "out").read_text() == "ALPHA\n"
    assert ".hidden\n" in (sub / "listing").read_text()
    result = batch("/usr/bin/yes | /usr/bin/head -n 1\n", cwd)
    assert result.returncode == 0 and result.stdout == "y\n", result
    result = batch("cd 'a dir' >cd.log\npwd\n", cwd, "/nonexistent")
    assert result.returncode == 0 and result.stdout == str(sub) + "\n", result
    assert (cwd / "cd.log").read_text() == ""
    for text, status, diagnostic in [
        ("lab2-nonexistent\n", 127, "lab2-nonexistent"),
        ("pwd |\n", 2, "missing command"),
        ("pwd >\n", 2, "filename"),
        ("pwd >> out\n", 2, "filename"),
        ("pwd & ls\n", 2, "last token"),
        ("pwd '\n", 2, "unclosed quote"),
        ("pwd >a >b\n", 2, "duplicate"),
        ("pwd " + "x " * 64 + "\n", 2, "too many arguments"),
        ("pwd|" * 16 + "pwd\n", 2, "too many pipeline stages"),
        ("x" * 9000 + "\n", 2, "overlong"),
        ("cd missing\n", 1, "missing"),
        ("ls missing\n", 1, "missing"),
        ("pwd <missing\n", 1, "missing"),
    ]:
        result = batch(text, cwd)
        assert result.returncode == status and diagnostic in result.stderr, result
    result = batch("pwd |\npwd\nexit\n", cwd)
    assert result.returncode == 0 and result.stdout == str(cwd) + "\n", result


def test_file_commands(cwd):
    (cwd / "first").write_text("apple\nbanana\n")
    (cwd / "second").write_text("apricot\n")
    (cwd / "target").mkdir()
    result = batch("cat first second | grep '^a' >matches\n"
                   "mv first second target\nrm target/first\n", cwd,
                   "/nonexistent")
    assert result.returncode == 0 and not result.stderr, result
    assert (cwd / "matches").read_text() == "apple\napricot\n"
    assert not (cwd / "target/first").exists()
    assert (cwd / "target/second").read_text() == "apricot\n"
    assert batch("grep absent target/second\n", cwd).returncode == 1
    assert batch("grep '[ ' target/second\n", cwd).returncode == 2
    assert batch("rm missing\n", cwd).returncode == 1


def test_terminal(cwd):
    pid, fd = pty.fork()
    if pid == 0:
        os.chdir(cwd)
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        os.execv(BINARY, [str(BINARY)])

    def until(needle):
        received = b""
        deadline = time.monotonic() + 4
        while needle not in received:
            remaining = deadline - time.monotonic()
            if remaining <= 0 or not select.select([fd], [], [], remaining)[0]:
                raise AssertionError((needle, received))
            received += os.read(fd, 8192)
        return received

    try:
        until(PROMPT)
        os.write(fd, b"/usr/bin/sleep 0.4 &\n")
        job = int(re.search(rb"\[(\d+)\]", until(PROMPT))[1])
        os.kill(job, 0)
        deadline = time.monotonic() + 3
        while pathlib.Path(f"/proc/{job}").exists():
            assert time.monotonic() < deadline, "background child not reaped"
            time.sleep(0.01)

        for key in [b"\x03", b"\x1c", b"\x1a"]:
            os.write(fd, b"/usr/bin/sleep 20 | /usr/bin/sleep 20\n")
            deadline = time.monotonic() + 3
            while os.tcgetpgrp(fd) == pid:
                assert time.monotonic() < deadline, "terminal not transferred"
                time.sleep(0.01)
            os.write(fd, key)
            output = until(PROMPT)
            if key == b"\x1a":
                assert b"stopped job terminated" in output, output
            # A successful pwd also verifies the shell recovered the terminal.
            os.write(fd, b"pwd\n")
            assert os.fsencode(cwd) in until(PROMPT)

        os.write(fd, b"partial\x03")
        until(PROMPT)
        os.write(fd, b"exit\n")
        deadline = time.monotonic() + 3
        while True:
            waited, status = os.waitpid(pid, os.WNOHANG)
            if waited:
                assert os.waitstatus_to_exitcode(status) == 130
                pid = None
                break
            assert time.monotonic() < deadline, "exit did not return"
            time.sleep(0.01)
    finally:
        if pid is not None:
            group = os.tcgetpgrp(fd)
            if group > 0 and group != pid:
                os.killpg(group, signal.SIGKILL)
            os.kill(pid, signal.SIGKILL)
            os.waitpid(pid, 0)
        os.close(fd)


with tempfile.TemporaryDirectory() as tmp:
    cwd = pathlib.Path(tmp).resolve()
    test_batch(cwd)
    print("PASS commands, cd, exit, redirection, pipes, parser errors")
    test_file_commands(cwd)
    print("PASS rm, mv, cat, grep")
    test_terminal(cwd)
    print("PASS background reaping and PTY Ctrl+C, Ctrl+\\\\, Ctrl+Z")
