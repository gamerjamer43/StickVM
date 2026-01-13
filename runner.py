from subprocess import run
from argparse import ArgumentParser
from pathlib import Path

from rich.console import Console
from rich.panel import Panel
from rich.table import Table

def main():
    a = ArgumentParser(description="Official set of StickVM opcode tests provided. If you do your own just reference the folder.")
    a.add_argument("-p", "--path", default="./vmm", help="Path to VM executable")
    a.add_argument("-d", "--dir", default="tests", help="Directory containing .stk files")
    a.add_argument("-c", "--cmd", default="", help="Command to prepend (e.g., 'valgrind --leak-check=full')")
    a.add_argument("-v", "--verbose", action="store_true", help="Show output from each test in real-time")

    # setup
    args = a.parse_args()
    console = Console()
    tests = sorted(Path(args.dir).glob("*.stk"))
    
    # console print returns none
    if not tests: return console.print("[red]No tests found![/]")

    passed, failed, results = [], [], []
    console.print(f"[yellow]Running {len(tests)} tests...[/]")
    valgrind = args.cmd and "valgrind" in args.cmd
    for test in tests:
        cmd = args.cmd.split() if args.cmd else []
        result = run(cmd + [args.path, str(test)], capture_output=not args.verbose, text=True)
        
        # valgrind returns 0, 1 instead
        if valgrind: code = result.returncode in (0, 1)
        else: code = result.returncode == 0
        
        if code:
            passed.append(test.name)
            console.print(f"[green]✓[/] {test.name}")
            results.append((test.name, "[green]PASS[/]", str(result.returncode)))

        else:
            failed.append((test.name, result.returncode, result.stdout, result.stderr))
            console.print(f"[red]✗[/] {test.name} (exit {result.returncode})")
            results.append((test.name, "[red]FAIL[/]", str(result.returncode)))

    # build nice table
    table = Table(title="Test Results")
    table.add_column("Test", style="cyan")
    table.add_column("Status", justify="center")
    table.add_column("Exit Code", justify="center")
    for name, status, code in results: table.add_row(name, status, code)
    table.add_row(f"[bold]Total: {len(tests)}[/]", f"[green]PASS: {len(passed)}[/]", f"[red]FAIL: {len(failed)}[/]")

    # print and (maybe) congratulate
    console.print(table)
    if not failed:
        console.print("[bold green]All tests passed![/]")
        return
    
    # log any fails and their output
    console.print(f"\n[bold red]{'='*50}[/]\n")
    for name, code, stdout, stderr in failed:
        console.print(Panel(f"[bold]{name}[/] (code: {code})", style="red", expand=False))
        if stdout: console.print(f"[yellow]stdout:[/] {stdout}")
        if stderr: console.print(f"[yellow]stderr:[/] {stderr}")

if __name__ == "__main__":
    main()