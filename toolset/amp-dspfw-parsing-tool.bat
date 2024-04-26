@echo off
prompt $
set execute=amp-dspfw-parser.exe
if exist "%execute%" (
%execute% -i %1 > %1.log
echo Pls look into %1.log for details
) else (
echo %execute% does not exist!
)
pause
prompt