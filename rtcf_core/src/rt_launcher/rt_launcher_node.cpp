#include "rt_launcher_node.hpp"

#include <signal.h>

#include <boost/program_options.hpp>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "ros/duration.h"
#include "ros/service.h"
#include "rtcf/LoadOrocosComponent.h"
#include "rtcf/Mapping.h"

RTLauncherNode::RTLauncherNode(const ros::NodeHandle &node_handle) : node_handle_(node_handle){};

int RTLauncherNode::loop() {
    ros::spin();
    return 0;
};

void RTLauncherNode::configure() { setupServiceClients(); };

void RTLauncherNode::shutdown() { shutdownServiceClients(); };

void RTLauncherNode::setupServiceClients() {
    ros::service::waitForService("/rt_runner/load_orocos_component", ros::Duration(-1));
    loadInRTRunnerClient = node_handle_.serviceClient<rtcf::LoadOrocosComponent>("/rt_runner/load_orocos_component");

    ros::service::waitForService("/rt_runner/unload_orocos_component", ros::Duration(-1));
    unloadInRTRunnerClient =
        node_handle_.serviceClient<rtcf::UnloadOrocosComponent>("/rt_runner/unload_orocos_component");
};

void RTLauncherNode::shutdownServiceClients() {
    loadInRTRunnerClient.shutdown();
    unloadInRTRunnerClient.shutdown();
};

rtcf::LoadOrocosComponent RTLauncherNode::genLoadMsg() {
    rtcf::LoadOrocosComponent srv;

    for (auto mapping : launcher_attributes_.mappings) {
        rtcf::Mapping m;

        m.from_topic.data = mapping.from_topic;
        m.to_topic.data   = mapping.to_topic;

        srv.request.mappings.push_back(m);
    }

    srv.request.ns.data             = launcher_attributes_.ns;
    srv.request.component_name.data = launcher_attributes_.name;

    srv.request.component_type.data          = launcher_attributes_.rt_type;
    srv.request.is_first.data                = launcher_attributes_.is_first;
    srv.request.is_sync.data                 = launcher_attributes_.is_sync;
    srv.request.topics_ignore_for_graph.data = launcher_attributes_.topics_ignore_for_graph;

    // ROS_INFO_STREAM("LOAD CALL:" << std::endl << srv.request);
    return srv;
};

rtcf::UnloadOrocosComponent RTLauncherNode::genUnloadMsg() {
    rtcf::UnloadOrocosComponent srv;

    srv.request.component_name.data = launcher_attributes_.name;
    srv.request.ns.data             = launcher_attributes_.ns;

    // ROS_INFO_STREAM("UNLOAD CALL:" << std::endl << srv.request);
    return srv;
};

bool RTLauncherNode::loadROSParameters() {
    // load parameters from ROS server
    // check if parameters are not overriden
    if (node_handle_.hasParam("is_first")) {
        if (launcher_attributes_.is_first) {
            ROS_ERROR("Conflicting configuration for parameter is_first");
            return false;
        } else {
            node_handle_.getParam("is_first", launcher_attributes_.is_first);
        }
    }
    if (node_handle_.hasParam("is_sync")) {
        if (launcher_attributes_.is_sync) {
            ROS_ERROR("Conflicting configuration for parameter is_sync");
            return false;
        } else {
            node_handle_.getParam("is_sync", launcher_attributes_.is_sync);
        }
    }

    if (node_handle_.hasParam("rt_type")) {
        if (!launcher_attributes_.rt_type.empty()) {
            ROS_ERROR("Conflicting configuration for parameter rt_type");
            return false;
        } else {
            node_handle_.getParam("rt_type", launcher_attributes_.rt_type);
        }
    }

    // check for RT type
    if (launcher_attributes_.rt_type.empty()) {
        ROS_ERROR("No RT type was specified!");
        return false;
    }

    // TODO: move this into RT Runner
    std::string topics_ignore_for_graph;
    if (node_handle_.getParam("topics_ignore_for_graph", topics_ignore_for_graph)) {
        launcher_attributes_.topics_ignore_for_graph = topics_ignore_for_graph;
    }

    return true;
}

bool RTLauncherNode::loadInRTRunner() {
    rtcf::LoadOrocosComponent srv = genLoadMsg();

    bool service_ok = loadInRTRunnerClient.call(srv);
    if (service_ok && srv.response.success.data) {
        ROS_DEBUG("RT Runner load called successfully");
        return true;
    } else {
        ROS_ERROR("Failed to call load service in RT Runner");
        return false;
    }
};

bool RTLauncherNode::unloadInRTRunner() {
    rtcf::UnloadOrocosComponent srv = genUnloadMsg();

    bool service_ok = unloadInRTRunnerClient.call(srv);
    if (service_ok && srv.response.success.data) {
        ROS_DEBUG("RT Runner unload called successfully");
        return true;
    } else {
        ROS_ERROR("Failed to call unload service in RT Runner");
        return false;
    }
};

void RTLauncherNode::loadNodeConfiguration() {
    std::string full_name     = ros::this_node::getName();
    launcher_attributes_.name = full_name.substr(full_name.rfind("/") + 1);
    launcher_attributes_.ns   = ros::this_node::getNamespace();
    ROS_DEBUG_STREAM("Got node name: " << launcher_attributes_.name);
    ROS_DEBUG_STREAM("Got node namespace: " << launcher_attributes_.ns);

    // get remaps
    const auto remappings = ros::names::getUnresolvedRemappings();
    for (const auto &pair : remappings) {
        Mapping mapping;
        mapping.from_topic = pair.first;
        mapping.to_topic   = pair.second;
        launcher_attributes_.mappings.push_back(mapping);
        ROS_DEBUG_STREAM("Got remapping from: [" << mapping.from_topic << "] to: [" << mapping.to_topic << "]");
    }
}

bool RTLauncherNode::handleArgs(int &argc, char **argv) {
    namespace po = boost::program_options;
    // handle options
    po::options_description desc("Allowed options");
    // clang-format off
    desc.add_options()
        ("help", "help message")
        ("is_first,f", "mark RT component as start component")
        ("is_sync,s", "mark RT component as sync component")
        ("rt_type", po::value<std::string>(), "name of the RT component to load");
    // clang-format on
    // rt_type can be given as positional argument
    po::positional_options_description p;
    p.add("rt_type", 1);

    // now parse the options
    po::variables_map vm;
    try {
        po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
        po::notify(vm);
    } catch (po::error e) {
        std::cout << e.what() << std::endl;
        return false;
    }

    // react appropriately
    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return false;
    }
    if (vm.count("is_first")) {
        launcher_attributes_.is_first = true;
    }
    if (vm.count("is_sync")) {
        launcher_attributes_.is_sync = true;
    }
    if (vm.count("rt_type")) {
        launcher_attributes_.rt_type = vm["rt_type"].as<std::string>();
    }
    return true;
}

void sigintHandler(int sig) {
    (void)sig;
    ROS_DEBUG("SIGINT handler called");
    node_ptr->unloadInRTRunner();
    node_ptr->shutdown();
    // after ros::shutdown is called, ros::spin() will return
    ros::shutdown();
}

int main(int argc, char **argv) {
    ros::init(argc, argv, "rt_launcher", ros::init_options::NoSigintHandler);
    ros::NodeHandle nh("~");

    node_ptr = std::make_unique<RTLauncherNode>(nh);
    signal(SIGINT, sigintHandler);

    if (!node_ptr->handleArgs(argc, argv)) {
        return EXIT_SUCCESS;
    }
    node_ptr->loadNodeConfiguration();
    if (!node_ptr->loadROSParameters()) {
        return EXIT_FAILURE;
    }
    node_ptr->configure();
    if (!node_ptr->loadInRTRunner()) {
        return EXIT_FAILURE;
    }
    node_ptr->loop();

    return 0;
}  // end main()
