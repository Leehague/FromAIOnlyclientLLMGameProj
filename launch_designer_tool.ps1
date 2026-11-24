param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ToolArgs
)

$script = Join-Path $PSScriptRoot "tools\designer_tool.py"
if (-not $ToolArgs -or $ToolArgs.Count -eq 0) {
    python $script ui
} else {
    python $script @ToolArgs
}
