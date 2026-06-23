@echo off
python clean_trace.py
if %ERRORLEVEL% NEQ 0 (
    echo Error during trace cleaning.
    exit /b %ERRORLEVEL%
)
java -jar "C:\Users\mouns\Master Studium\tessla-assembly-2.1.0.jar" interpreter tessla/verification.tessla dos_Tessla_filtered.txt
