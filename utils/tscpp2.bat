@rem Windows wrapper for tscpp2 (a #!/usr/bin/perl script with no extension).
@rem cmd.exe cannot honour a shebang, so callers that invoke `tscpp2` from cmd
@rem (e.g. ninja's `cmd /C ...` on a native MinGW build) resolve this .bat via
@rem PATHEXT and it runs the sibling perl script through perl.  MSYS2 / Cygwin
@rem bash still runs the extensionless script directly via its shebang, so no
@rem caller has to change how it invokes tscpp2.  Windows-port design memo.
@perl "%~dp0tscpp2" %*
