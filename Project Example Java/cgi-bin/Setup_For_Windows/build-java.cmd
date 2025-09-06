@echo off
setlocal
cd /d "%~dp0"

if not exist lib (
  echo [!] Put mariadb-java-client-*.jar in %cd%\lib\ first.
  pause
  exit /b 1
)

mkdir classes 2>nul

set "DRIVERJAR="
for %%J in (lib\mariadb-java-client-*.jar) do set "DRIVERJAR=%%J"

if "%DRIVERJAR%"=="" (
  echo [!] No MariaDB JDBC jar found in lib\.
  echo     Download from https://mariadb.com/downloads/connectors/java/
  pause
  exit /b 1
)

echo [*] Compiling...
"%JAVA_HOME%\bin\javac.exe" -cp "%DRIVERJAR%" -d classes Auth.java Listings.java BidSell.java Transactions.java || goto :err

echo [*] Creating wrappers...
>auth.cmd        echo @echo off & echo setlocal & echo set "ROOT=%%~dp0" & echo set "CLASSPATH=%%ROOT%%classes;%%ROOT%%lib\*" & echo "%%JAVA_HOME%%\bin\java.exe" Auth & echo endlocal
>listings.cmd    echo @echo off & echo setlocal & echo set "ROOT=%%~dp0" & echo set "CLASSPATH=%%ROOT%%classes;%%ROOT%%lib\*" & echo "%%JAVA_HOME%%\bin\java.exe" Listings & echo endlocal
>bid_sell.cmd    echo @echo off & echo setlocal & echo set "ROOT=%%~dp0" & echo set "CLASSPATH=%%ROOT%%classes;%%ROOT%%lib\*" & echo "%%JAVA_HOME%%\bin\java.exe" BidSell & echo endlocal
>transactions.cmd echo @echo off & echo setlocal & echo set "ROOT=%%~dp0" & echo set "CLASSPATH=%%ROOT%%classes;%%ROOT%%lib\*" & echo "%%JAVA_HOME%%\bin\java.exe" Transactions & echo endlocal

echo [✓] Done. Visit:
echo     http://localhost/cgi-bin/auth.cmd
echo     http://localhost/cgi-bin/listings.cmd
echo     http://localhost/cgi-bin/bid_sell.cmd
echo     http://localhost/cgi-bin/transactions.cmd
exit /b 0

:err
echo [x] Build failed.
pause
exit /b 1
