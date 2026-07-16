include(${PROJECT_SOURCE_DIR}/cmake/AddTinyStateLibrary.cmake)


set(TS2_SOURCES
  classes/ts2/c++/ts2IOsockTCPserver.cpp
  classes/ts2/c++/ts2IOsockUnixServer.cpp
  classes/ts2/c++/ts2Parallel.cpp
  classes/ts2/c++/stdSemaphore.cpp
  classes/ts2/c++/stdString.cpp
  classes/ts2/c++/tsFinishChecker.cpp
  classes/ts2/c++/stdBuffer.cpp
  classes/ts2/c++/tsThread.cpp
  classes/ts2/c++/stdLimitSemaphore.cpp
  classes/ts2/c++/ts2IOunixClient.cpp
  classes/ts2/c++/stdObject.cpp
  classes/ts2/c++/sThreadMutexRecursive.cpp
  classes/ts2/c++/stdInterval.cpp
  classes/ts2/c++/sArray.cpp
  classes/ts2/c++/tsCallBuffer.cpp
  classes/ts2/c++/ts2IOsockUDP.cpp
  classes/ts2/c++/tsApplication.cpp
  classes/ts2/c++/stdMutex.cpp
  classes/ts2/c++/ts2IOraw.cpp
  classes/ts2/c++/sBufferHandle.cpp
  classes/ts2/c++/stdEventHandle.cpp
  classes/ts2/c++/tinyState.cpp
  classes/ts2/c++/stdHalfOrderQueue.cpp
  classes/ts2/c++/tsSignal.cpp
  classes/ts2/c++/stdStringTree.cpp
  classes/ts2/c++/sException.cpp
  classes/ts2/c++/ts2IO.cpp
  classes/ts2/c++/stdRx.cpp
  classes/ts2/c++/ts2IOdevNull.cpp
  classes/ts2/c++/stdHalfOrderQueueTS.cpp
  classes/ts2/c++/stdEvent.cpp
  classes/ts2/c++/sThreadCond.cpp
  classes/ts2/c++/sEndian.cpp
  classes/ts2/c++/sObject.cpp
  classes/ts2/c++/ts2IOsockTCPconnect.cpp
  classes/ts2/c++/tsGC.cpp
  classes/ts2/c++/sThreadMutexHandle.cpp
  classes/ts2/c++/stdQueueString.cpp
  classes/ts2/c++/stdQueue.cpp
  classes/ts2/c++/stdFrameWork.cpp
  classes/ts2/c++/stdBalancedTree.cpp
  classes/ts2/c++/stdArray.cpp
  classes/ts2/c++/sCallSection.cpp
  classes/ts2/c++/co_tsThreadKill.cpp
  classes/ts2/c++/tsCallList.cpp
  )

set(TS2_ARCH_SOURCES
  classes/ts2/c++/tsSignalCore.cpp
  classes/ts2/c++/fwIO.cpp
  classes/ts2/c++/ts2IOdescriptor.cpp
  classes/ts2/c++/ts2IOsocket.cpp
  classes/ts2/c++/ts2System.cpp
  classes/ts2/c++/s2IOstd.cpp
  classes/ts2/c++/ts2IOsockServer.cpp
)

if(APPLE)
  list(APPEND TS2_ARCH_SOURCES classes/ts2/c++/stdOSfundamental.cpp)
endif()
if(WIN32)
  # console ts2IO exists only on Windows (posix_MinGW overlay)
  list(APPEND TS2_ARCH_SOURCES classes/ts2/c++/ts2IOwinConsole.cpp)
endif()


add_tinystate_library(
  NAME tinyState2
  SOURCES
  # --- ts2 ---
  ${TS2_SOURCES}

  ARCH_SOURCES
  ${TS2_ARCH_SOURCES}

  DARWIN_SOURCES
)
