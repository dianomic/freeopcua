/// @author Alexander Rykovanov 2013
/// @email rykovanov.as@gmail.com
/// @brief Remote Computer implementaion.
/// @license GNU LGPL
///
/// Distributed under the GNU LGPL License
/// (See accompanying file LICENSE or copy at
/// http://www.gnu.org/licenses/lgpl.html)
///
/// An example program designed to talk to FogLAMP
/// using the foglamp-north-opcua plugin

#include <opc/ua/client/client.h>
#include <opc/ua/node.h>
#include <opc/ua/subscription.h>

#include <opc/common/logger.h>

#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace OpcUa;

class SubClient : public SubscriptionHandler
{
  void DataChange(uint32_t handle, const Node & node, const Variant & val, AttributeId attr) override
  {
    std::cout << "Received DataChange event, value of Node " << node << " is now: "  << val.ToString() << std::endl;
  }
};

void browseTree(std::shared_ptr<spdlog::logger> logger, Node& node, int level)
{
	char buf[132];
	QualifiedName nName = node.GetBrowseName();
	snprintf(buf, sizeof(buf), "%*s node(%d:%s)", level * 8, " ", nName.NamespaceIndex, nName.Name.c_str());
	logger->info(buf);
	std::vector<OpcUa::Node> variables = node.GetVariables();
	if (variables.size() > 0)
	{
		snprintf(buf, sizeof(buf), "%*sNode (%d:%s) has %lu variables",
					   level * 8, " ",
                                                   nName.NamespaceIndex,
                                                   nName.Name.c_str(),
                                                   variables.size());
		logger->info(buf);
	}
	for (auto var : variables)
	{
		OpcUa::QualifiedName qName = var.GetBrowseName();
		snprintf(buf, sizeof(buf), "%*s variable(%d:%s)", level * 8 + 4, " ", qName.NamespaceIndex, qName.Name.c_str());
		logger->info(buf);
	}
	std::vector<OpcUa::Node> children = node.GetChildren();
	snprintf(buf, sizeof(buf), "%*sNode (%d:%s) has %lu children",
					   level * 8, " ",
                                           nName.NamespaceIndex,
                                           nName.Name.c_str(),
                                           children.size());
	logger->info(buf);

	for (auto child : children)
	{
		browseTree(logger, child, level + 1);
	}

}


int main(int argc, char ** argv)
{
  auto logger = spdlog::stderr_color_mt("client");
 // logger->set_level(spdlog::level::debug);
  try
    {
      //std::string endpoint = "opc.tcp://192.168.56.101:48030";
      //std::string endpoint = "opc.tcp://user:password@192.168.56.101:48030";
      // std::string endpoint = "opc.tcp://localhost:4840/foglamp/server";
      std::string endpoint = "opc.tcp://127.0.0.1:4840/freeopcua/server/";
      //std::string endpoint = "opc.tcp://localhost:53530/OPCUA/SimulationServer/";
      //std::string endpoint = "opc.tcp://localhost:48010";

      if (argc > 1)
        { endpoint = argv[1]; }

      logger->info("Connecting to: {}", endpoint);

      OpcUa::UaClient client(logger);
      client.Connect(endpoint);

      //get Root node on server
      OpcUa::Node root = client.GetRootNode();
      logger->info("Root node is: {}", root);
      logger->info("Children of root node are:");
      for (OpcUa::Node node : root.GetChildren())
        { logger->info("    {}", node); }

      logger->info("Browse tree");
      browseTree(logger, root, 0);

      //get and browse Objects node
      logger->info("Children of objects node are:");
      Node objects = client.GetObjectsNode();

      for (OpcUa::Node node : objects.GetChildren())
        { logger->info("    {}", node); }

      logger->info("Browse objects tree");
      browseTree(logger, objects, 0);

      //get a node from standard namespace using objectId
      logger->info("NamespaceArray is:");
      OpcUa::Node nsnode = client.GetNode(ObjectId::Server_NamespaceArray);
      OpcUa::Variant ns = nsnode.GetValue();

      for (std::string d : ns.As<std::vector<std::string>>())
        { logger->info("    {}", d); }

      OpcUa::Node myvar;
      OpcUa::Node myobject;
      OpcUa::Node mymethod;

      //Initialize Node myvar:

      //Get namespace index we are interested in

      // From freeOpcUa Server:
      //uint32_t idx = client.GetNamespaceIndex("http://examples.freeopcua.github.io");
      uint32_t idx = client.GetNamespaceIndex("http://foglamp.dianomic.com");
      ////Get Node using path (BrowsePathToNodeId call)
      //std::vector<std::string> varpath({ std::to_string(idx) + ":NewObject", "MyVariable" });
      //myvar = objects.GetChild(varpath);
      std::vector<std::string> methodpath({ std::to_string(idx) + ":NewObject" });
      myobject = objects.GetChild(methodpath);
      methodpath = { std::to_string(idx) + ":NewObject", "MyMethod" };
      mymethod = objects.GetChild(methodpath);
      std::vector<OpcUa::Variant> arguments;
      arguments.push_back(static_cast<uint8_t>(0));
      myobject.CallMethod(mymethod.GetId(), arguments);

      // Example data from Prosys server:
      //std::vector<std::string> varpath({"Objects", "5:Simulation", "5:Random1"});
      //myvar = root.GetChild(varpath);

      // Example from any UA server, standard dynamic variable node:
      std::vector<std::string> varpath{ "Objects", "Server", "ServerStatus", "CurrentTime" };
      myvar = root.GetChild(varpath);

      logger->info("got node: {}", myvar);

      //Subscription
      SubClient sclt;
      Subscription::SharedPtr sub = client.CreateSubscription(100, sclt);
      uint32_t handle = sub->SubscribeDataChange(myvar);
      logger->info("Got sub handle: {}, sleeping 5 seconds", handle);
      std::this_thread::sleep_for(std::chrono::seconds(5));

      logger->info("Disconnecting");
      client.Disconnect();
      logger->flush();
      return 0;
    }

  catch (const std::exception & exc)
    {
      logger->error("Error: {}", exc.what());
    }

  catch (...)
    {
      logger->error("Unknown error.");
    }

  return -1;
}

