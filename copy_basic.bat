@echo off
del "C:\Program Files (x86)\Steam\steamapps\infectiousfight\garry's mod beta\garrysmodbeta\lua\bin\gmsv_hello.dll"
del "C:\Program Files (x86)\Steam\steamapps\infectiousfight\garry's mod beta\garrysmodbeta\lua\bin\gmcl_hello.dll"
copy gmsv_hello.dll "C:\Program Files (x86)\Steam\steamapps\infectiousfight\garry's mod beta\garrysmodbeta\lua\bin\gmsv_hello.dll"
copy gmcl_hello.dll "C:\Program Files (x86)\Steam\steamapps\infectiousfight\garry's mod beta\garrysmodbeta\lua\bin\gmcl_hello.dll"
del  gmsv_hello.dll
del  gmcl_hello.dll