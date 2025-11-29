param(
    [string]$mode = ""
)

$dll = "NppVim.dll"

function Build-Target($triple, $msvcArch, $folderName) {
    Write-Host "`n=== Building $triple ==="

    Add-Msvc $msvcArch

    cargo build --release --target $triple

    $outDir = "dist/$folderName"
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null

    $src = "target/$triple/release/$dll"
    $dst = "$outDir/$dll"

    if (Test-Path $src) {
        Copy-Item $src $dst -Force
        Compress-Archive -LiteralPath $outDir `
            -DestinationPath "dist/$folderName.zip" -Force

        Write-Host "✓ Built and packaged: dist/$folderName.zip"
    } else {
        Write-Host "✗ ERROR: DLL not found for $triple"
    }
}

if ($mode -eq "release") {
    New-Item -ItemType Directory -Force -Path "dist" | Out-Null

    Build-Target "i686-pc-windows-msvc"    "x86"   "NppVim"
    Build-Target "x86_64-pc-windows-msvc"  "x64"   "NppVim.x64"
    Build-Target "aarch64-pc-windows-msvc" "arm64" "NppVim.arm64"

    exit
}

Write-Host "`n Debug x64 Build"
Add-Msvc "x64"

cargo build --target x86_64-pc-windows-msvc

$debugPath = "target/x86_64-pc-windows-msvc/debug/$dll"
Write-Host "`nx64 debug build:"
Write-Host $debugPath
