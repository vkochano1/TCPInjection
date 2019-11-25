
#include <Session.h>

int main()
{
  Device device;

  std::string localIP ("192.168.2.2");
  size_t localPort = 5555;

  std::string destIP  ("192.168.2.3");
  size_t destPort = 5556;

  Tins::NetworkInterface iface(Tins::IPv4Address(localIP.c_str()));
  auto resolvedMAC = Utils::resolveMAC(destIP, iface);

  SessionConfig sessionCfg ( iface.hw_address().to_string(), iface.ipv4_address().to_string(), localPort
                            , resolvedMAC.to_string(),       destIP,  destPort);
  Session session (device, sessionCfg);
  session .init();
  while(1)
  {
    session.poll();
  }

  return 0;
}
