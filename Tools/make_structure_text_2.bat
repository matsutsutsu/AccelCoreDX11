@echo off
setlocal enabledelayedexpansion

set "OUTPUT=folder_structure.txt"

echo === Folder Structure Report === > "%OUTPUT%"
echo Target Directory: %cd% >> "%OUTPUT%"
echo ---------------------------------------- >> "%OUTPUT%"
echo. >> "%OUTPUT%"

rem ================================
rem  Level 0（カレントフォルダ）
rem ================================
echo [Level 0] Current Folder Contents >> "%OUTPUT%"
for /f "delims=" %%A in ('dir /b') do (
    if exist "%%A\" (
        echo - [Dir ] %%A >> "%OUTPUT%"
    ) else (
        echo - [File] %%A >> "%OUTPUT%"
    )
)
echo. >> "%OUTPUT%"

rem ================================
rem  Level 1（1階層）
rem ================================
echo [Level 1] Subfolders >> "%OUTPUT%"
for /f "delims=" %%D in ('dir /b /ad') do (
    echo ▼ [Dir ] %%D >> "%OUTPUT%"

    for /f "delims=" %%A in ('dir /b "%%D"') do (
        if exist "%%D\%%A\" (
            echo    - [Dir ] %%A >> "%OUTPUT%"
        ) else (
            echo    - [File] %%A >> "%OUTPUT%"
        )
    )
    echo. >> "%OUTPUT%"
)
echo. >> "%OUTPUT%"

rem ================================
rem  Level 2（2階層）
rem ================================
echo [Level 2] Sub-subfolders >> "%OUTPUT%"
for /f "delims=" %%D in ('dir /b /ad') do (
    for /f "delims=" %%E in ('dir /b /ad "%%D"') do (
        echo ▼ [Dir ] %%D\%%E >> "%OUTPUT%"

        for /f "delims=" %%A in ('dir /b "%%D\%%E"') do (
            if exist "%%D\%%E\%%A\" (
                echo    - [Dir ] %%A >> "%OUTPUT%"
            ) else (
                echo    - [File] %%A >> "%OUTPUT%"
            )
        )
        echo. >> "%OUTPUT%"
    )
)
echo. >> "%OUTPUT%"

rem ================================
rem  Level 3（3階層）
rem ================================
echo [Level 3] Sub-sub-subfolders >> "%OUTPUT%"
for /f "delims=" %%D in ('dir /b /ad') do (
    for /f "delims=" %%E in ('dir /b /ad "%%D"') do (
        for /f "delims=" %%F in ('dir /b /ad "%%D\%%E"') do (
            echo ▼ [Dir ] %%D\%%E\%%F >> "%OUTPUT%"

            for /f "delims=" %%A in ('dir /b "%%D\%%E\%%F"') do (
                if exist "%%D\%%E\%%F\%%A\" (
                    echo    - [Dir ] %%A >> "%OUTPUT%"
                ) else (
                    echo    - [File] %%A >> "%OUTPUT%"
                )
            )
            echo. >> "%OUTPUT%"
        )
    )
)
echo. >> "%OUTPUT%"

echo 完了しました。 "%OUTPUT%" を確認してください。
pause
