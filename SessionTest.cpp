
/*#include <iostream>
#include <sstream>
#include <tins/ethernetII.h>
#include <tins/tins.h>
#include <queue>
#include <arpa/inet.h>*/
#include <Session.h>


int main()
{
  Device device;

  SessionConfig sessionCfg ( "7c:fe:90:81:4f:b9", "192.168.2.2", 5555
                            , "24:8a:07:58:cf:dc", "192.168.2.3", 5556);
  Session session (device, sessionCfg);
  session .init();
  while(1)
  {
    session.poll();
  }

  return 0;
}
