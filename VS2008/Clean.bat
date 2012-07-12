call "%VS90COMNTOOLS%..\..\VC\vcvarsall.bat"
devenv /nologo Supermodel.sln /Clean
if %ERRORLEVEL% neq 0 (
@echo Clean failed
pause
exit
)
