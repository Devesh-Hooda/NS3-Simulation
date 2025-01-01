#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/error-model.h"
#include "ns3/traffic-control-module.h"  

using namespace ns3;


// Ensure NS3 and NetAnim is installed into your system 
// Setup Log environment as Logs using Segment  -> NS_LOG_COMPONENT_DEFINE("%name%");
 
NS_LOG_COMPONENT_DEFINE("name");

// switch out name after you have created the Environment using the above segment and replaced with %name% 



// infinite loop structure - constraint within NetAnim
int totalLoops = 0;  

void SendPacket(NodeContainer routers, std::vector<Ipv4Address> serverAddresses, uint32_t routerIndex, uint32_t loopCount)
{
    for (uint32_t j = 0; j < serverAddresses.size(); ++j)
    {
        UdpEchoClientHelper echoClient(serverAddresses[j], 9);
        
        // edit value for (packet per cycle)
        echoClient.SetAttribute("MaxPackets", UintegerValue(1));  
        
        // edit value (in seconds) for custom time intervals
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));  
        
        // Simulate packet size -> 256 bytes (2048 bits)
        echoClient.SetAttribute("PacketSize", UintegerValue(256));  
        ApplicationContainer clientApps = echoClient.Install(routers.Get(routerIndex));
        
        // Change destination every 5 seconds
        clientApps.Start(Seconds(2.0 + routerIndex + j * 5));  
        
        //Duration of simulation -> NS3 controllable but here defined too
        clientApps.Stop(Seconds(120.0)); 
    }

    // Increment loop count and reschedule the packet sending process
    loopCount++;
    totalLoops++;
    
    // Reschedule infinite time (every 5 seconds)
    Simulator::Schedule(Seconds(5.0), &SendPacket, routers, serverAddresses, routerIndex, loopCount);
}

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);
    
    // Ensure Logging environment is same every instance 
    LogComponentEnable("name", LOG_LEVEL_INFO);

    NodeContainer routers, servers;
    routers.Create(4);  // 4 routers
    servers.Create(8);  // 8 servers

    InternetStackHelper internet;
    internet.Install(routers);
    internet.Install(servers);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    
    // Propagation delay: 1ms
    p2p.SetChannelAttribute("Delay", StringValue("1ms")); 

    Ipv4AddressHelper address;

    std::vector<std::pair<NodeContainer, std::string>> routerLinks = {
        {NodeContainer(routers.Get(2), routers.Get(3)), "10.0.1.0"} 
    };

    for (auto &link : routerLinks) {
        NetDeviceContainer devices = p2p.Install(link.first);

        Ptr<RateErrorModel> errorModel = CreateObject<RateErrorModel>();
        
        // 0.5% error simulation portion
        errorModel->SetAttribute("ErrorRate", DoubleValue(0.005));
        devices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(errorModel));

        address.SetBase(Ipv4Address(link.second.c_str()), "255.255.255.0");
        address.Assign(devices);
    }

    std::vector<Ipv4Address> serverAddresses;
    
    // hardcoded server links
    NodeContainer serverLinks[] = {
        NodeContainer(routers.Get(0), servers.Get(0)), 
        NodeContainer(routers.Get(0), servers.Get(1)), 
        NodeContainer(routers.Get(1), servers.Get(4)), 
        NodeContainer(routers.Get(1), servers.Get(5)), 
        NodeContainer(routers.Get(2), servers.Get(2)), 
        NodeContainer(routers.Get(2), servers.Get(3)), 
        NodeContainer(routers.Get(3), servers.Get(6)), 
        NodeContainer(routers.Get(3), servers.Get(7))  
    };

    for (uint32_t i = 0; i < 8; ++i) {
        NetDeviceContainer devices = p2p.Install(serverLinks[i]);

        Ptr<RateErrorModel> errorModel = CreateObject<RateErrorModel>();
        errorModel->SetAttribute("ErrorRate", DoubleValue(0.005)); 
        devices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(errorModel));

        std::ostringstream subnet;
        subnet << "192.168." << i + 1 << ".0";
        address.SetBase(Ipv4Address(subnet.str().c_str()), "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);
        serverAddresses.push_back(interfaces.GetAddress(1));
    }

    std::vector<std::pair<NodeContainer, std::string>> serverLinksCube = {
        {NodeContainer(servers.Get(0), servers.Get(1)), "192.168.9.0"},
        {NodeContainer(servers.Get(0), servers.Get(4)), "192.168.10.0"}, 
        {NodeContainer(servers.Get(1), servers.Get(5)), "192.168.11.0"}, 
        {NodeContainer(servers.Get(1), servers.Get(3)), "192.168.12.0"},
        {NodeContainer(servers.Get(2), servers.Get(3)), "192.168.13.0"},
        {NodeContainer(servers.Get(2), servers.Get(6)), "192.168.14.0"},
        {NodeContainer(servers.Get(3), servers.Get(7)), "192.168.15.0"}, 
        {NodeContainer(servers.Get(4), servers.Get(5)), "192.168.16.0"},
        {NodeContainer(servers.Get(4), servers.Get(6)), "192.168.17.0"},
        {NodeContainer(servers.Get(5), servers.Get(7)), "192.168.18.0"},
        {NodeContainer(servers.Get(6), servers.Get(7)), "192.168.19.0"}  
    };

    for (auto &link : serverLinksCube) {
        NetDeviceContainer devices = p2p.Install(link.first);

        Ptr<RateErrorModel> errorModel = CreateObject<RateErrorModel>();
        errorModel->SetAttribute("ErrorRate", DoubleValue(0.005));  // Packet drop rate: 0.5%
        devices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(errorModel));

        address.SetBase(Ipv4Address(link.second.c_str()), "255.255.255.0");
        address.Assign(devices);
    }

    for (uint32_t i = 0; i < servers.GetN(); ++i) {
        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApps = echoServer.Install(servers.Get(i));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(100.0));
    }

    for (uint32_t i = 0; i < routers.GetN(); ++i) {
        SendPacket(routers, serverAddresses, i, 0); 
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    p2p.EnablePcapAll("modified-topology");
    
    // customise name to be further run into NETANIM
    AnimationInterface anim("yes4.xml");

    Simulator::Run(); 
    Simulator::Destroy();

    return 0;
}

