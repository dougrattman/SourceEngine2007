:: Copy developer tools artifacts to devtools/bin.

@echo on
setlocal

:: Make sure we have all args
if %5 == "" (
  echo  Post-Build: ERROR: Incorrect number of args provided. Usage post_build_copy_artifacts_to_devtools.cmd "$(SolutionDir)" "$(TargetDir)" "$(TargetPath)" "$(TargetName)" "$(TargetFileName)" [bin dir].
  endlocal
  exit /b 1
)

set solution_dir=%~1
set target_dir=%~2
set target_path=%~3
set target_name=%~4
set target_file_name=%~5
set bin_dir=%~6
set "full_result_dir=%solution_dir%devtools\%bin_dir%\"

if not exist "%full_result_dir%" (
  md "%full_result_dir%"
)

if ERRORLEVEL 0 (
  copy /V /B /Y "%target_path%" "%full_result_dir%%target_file_name%"
)

if ERRORLEVEL 0 (
  if exist "%target_dir%%target_name%.map" (
    copy /V /A /Y "%target_dir%%target_name%.map" "%full_result_dir%%target_name%.map"
  )
)

if ERRORLEVEL 0 (
  if exist "%target_dir%%target_name%.pdb" (
    copy /V /B /Y "%target_dir%%target_name%.pdb" "%full_result_dir%%target_name%.pdb"
  )
)

if NOT ERRORLEVEL 0 (
  echo Post-Build: ERROR: Failed copying %target_file_name% artifacts. Ensure game is not running.
  del /q "%target_path%"
)

endlocal

exit /B ERRORLEVEL