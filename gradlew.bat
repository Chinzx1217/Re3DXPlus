@echo off
set DIR=%~dp0
if "%JAVA_HOME%"=="" (
  set JAVA=java.exe
) else (
  set JAVA="%JAVA_HOME%\bin\java.exe"
)
%JAVA% -Dorg.gradle.appname=gradlew -classpath "%DIR%gradle\wrapper\gradle-wrapper.jar" org.gradle.wrapper.GradleWrapperMain %*
exit /b %ERRORLEVEL%
