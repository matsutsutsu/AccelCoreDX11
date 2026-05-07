@echo off
set OUTPUT=merged.txt
echo --- ソースまとめ開始 --- > %OUTPUT%

for %%f in (*.h *.cpp) do (
    echo. >> %OUTPUT%
    echo =============================== >> %OUTPUT%
    echo ファイル: %%f >> %OUTPUT%
    echo =============================== >> %OUTPUT%
    type "%%f" >> %OUTPUT%
)

echo 完了しました。
