@echo off
setlocal enabledelayedexpansion

REM 出力フォルダ名
set "OUTDIR=Text"

REM 出力フォルダがなければ作成
if not exist "%OUTDIR%" (
    mkdir "%OUTDIR%"
)

REM 再帰的に cpp / h / hlsl などを走査
for /R %%F in (*.cpp *.c *.h *.hpp *.hlsl) do (
    REM 元ファイル名（拡張子付き）
    set "NAME=%%~nxF"

    REM 拡張子のドットをアンダースコアに変える（.cpp → _cpp）
    set "NAME=!NAME:.=_!"

    REM 出力ファイルパス
    set "OUTFILE=%OUTDIR%\!NAME!.txt"

    echo Converting: %%F
    type "%%F" > "!OUTFILE!"
)

echo.
echo 変換が完了しました。
echo "%OUTDIR%" フォルダの中を確認してください。
pause
