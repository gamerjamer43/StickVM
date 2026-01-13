"""
unified test runner for anything test.py gens.
will run any .stk file in the folder, reporting back any errors and its stdout/stderr
(which i should prolly put the code in stderr but... we'll do that later)
"""
import subprocess
from pathlib import Path
from rich.console import Console
from rich.table import Table
from rich.panel import Panel

console = Console()
PATH: str = "./vm.exe"
DIR = Path("tests")

def main():
    # setup
    passed, failed, results = [], [], []
    tests = sorted(DIR.glob("*.stk"))
    if not tests: return console.print("[red]No tests found![/]")

    console.print(f"[yellow]Running {len(tests)} tests...[/]")
    for test in tests:
        # steal output so we can use it later
        result = subprocess.run(
            [PATH, str(test)],
            capture_output=True,
            text=True,
        )

        # return 0 == pass (except for panic)
        check: bool = result.returncode == 0
        if check or test.name.startswith("testop1_") and not check:
            passed.append(test.name)
            console.print(f"[green]✓[/] {test.name}")
            results.append((test.name, "[green]PASS[/]", str(result.returncode)))

        # any other == fail. 
        # TODO: gonna make actual error logs next... maybe stack tracing?
        # a lot of error handling has to be done at compile time
        else:
            failed.append((test.name, result.returncode, result.stdout, result.stderr))
            console.print(f"[red]✗[/] {test.name} (exit {result.returncode})")
            results.append((test.name, "[red]FAIL[/]", str(result.returncode)))

    # cool results table tanks rich
    table = Table(title="Test Results")
    table.add_column("Test", style="cyan")
    table.add_column("Status", justify="center")
    table.add_column("Exit Code", justify="center")
    for name, status, code in results:
        table.add_row(name, status, code)
    
    # finishing touches and cough it out
    total = len(tests)
    table.add_row(
        f"[bold]Total: {total}[/]",
        f"[green]PASS: {len(passed)}[/]",
        f"[red]FAIL: {len(failed)}[/]"
    )

    # good job!
    if not failed: 
        console.print(table)
        console.print("[bold green]All tests passed![/]")
        return
    
    # failure will not be tolerated
    console.print(f"\n[bold red]{'='*50}[/]\n")
    console.print(table)
    for name, code, stdout, stderr in failed:
        console.print(Panel(f"[bold]{name}[/] (code: {code})", style="red", expand=False))

        # any console info
        if stdout: console.print(f"[yellow]stdout:[/] \n{stdout}")
        if stderr: console.print(f"[yellow]stdout:[/] \n{stderr}")

if __name__ == "__main__":
    main()