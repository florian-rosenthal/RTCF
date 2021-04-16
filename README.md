# Real-Time Control Framework

The Real-Time Control Framework (RTCF) provides an easy way of developing modular and real-time safe controller architectures that can be seamlessly integrated within a Robot Operating System (ROS) environment. Minimum learning effort is required for users that are familiar with ROS.

## About the Project

In recent years, ROS evolved to one of the de-facto standards in robotics research. Although control systems are an inherent part of any robotic application, their implementation in ROS is somewhat limited due to the concept of nodes running in parallel. In the past, several attempts have been made to mitigate those issues. A very popular among them is *ros_control*. However, this library has some shortcomings, mainly including missing modularity and a steep learning curve as well.

For this reason, we developed a new software package based on *OROCOS* and *rtt_ros_integration* that we call the *Real-Time Control Framework* (RTCF). The features include:

- High modularity (reusable components and configurable at run-time, compatible with *roslaunch*)
- High-performance controller execution (> 1 kHz possible)
- Fundamentally real-time safe
- Real-time safe logging (compatible with *rosconsole*)
- Real-time safe dynamic parameters (compatible with *dynamic_reconfigure*)
- Seamless integration into an existing ROS-ecosystem
- Almost no learning required for ROS-developers

## Getting Started

This section described how you can get and build a copy of the RTCF.

### Prerequisites

Currently, RTCF is developed in the following environment:

- ROS Melodic
- Ubuntu 18.04

Chances are good that the RTCF also works on ROS Noetic and ROS Kinetic, although this is not tested.

For real-time execution, the [PREEMPT_RT kernel patch](https://wiki.linuxfoundation.org/realtime/documentation/howto/applications/preemptrt_setup) is used. Please note, that the real-time performance of your controller will heavily depend on the proper setup of PREEMPT_RT. However, your controllers can also run on a non-real-time system if reduced performance (e.g., jitter, missed deadlines) is acceptable.

### Installation

The main dependencies of this project are *OROCOS* and *rtt_ros_integration*. For a convenient setup of those, a script is provided. Simply create an empty ROS workspace and execute the following in a terminal:

```bash
./setup_orocos path_to/created_workspace/
```

If you are worried about typing your password, just have a look at the script file. `sudo` is only required for installing missing packages. It takes 5-30 minutes to build everything. Don't forget to follow the instruction in the last output line of the script.

Now, the RTCF can be built. To do this, this repo must be cloned into your project's ROS workspace:

```bash
git clone https://github.com/KIT-ISAS/RTCF path_to_project_workspace/src/RTCF
```

If everything is set up correctly, your preferred build command for catkin packages (`catkin_make` or `catkin build`) will now build RTCF like any other ROS package.

## Usage

The RTCF is designed to be used as simply as possible by ROS users. However, some new terminology appears due to the usage of OROCOS behind the scenes. Here is some translation guide from OROCOS to ROS:

- component: node or nodelet
- input port: subscriber
- output port: publisher

In the following, the basic concepts of the RTCF, that are required for component development and deployment are required. Furthermore, it is highly recommended to have a look at the code in the [rtcf_examples-package](rtcf_examples/).

### Components

Each module of a complex control architecture is reflected as a component in the RTCF. A component is basically a library that lives in a ROS package and that is loaded at runtime and connected with other components or ROS topics to perform its task.

To create a component `SimpleComponent` in the package `simple_component` the following code is needed:

`CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.0.2)
project(simple_component)

find_package(OROCOS-RTT REQUIRED)
include(${OROCOS-RTT_USE_FILE_PATH}/UseOROCOS-RTT.cmake)
orocos_use_package(ocl-logging)

find_package(catkin REQUIRED COMPONENTS rtcf std_msgs)

include_directories(${catkin_INCLUDE_DIRS})
orocos_component(simple_component src/component.cpp)
target_link_libraries(simple_component ${catkin_LIBRARIES})

orocos_generate_package()
catkin_package()
```

`package.xml`:

```xml
<package>
  <name>simple_component</name>
  <version>0.0.1</version>
  <description>RTCF tutorial</description>
  <maintainer email="michael.fennel@kit.edu">Michael Fennel</maintainer>
  <license>MIT</license>

  <buildtool_depend>catkin</buildtool_depend>
  <build_depend>rtcf</build_depend>
  <build_depend>std_msgs</build_depend>
  <run_depend>rtcf</run_depend>
  <run_depend>std_msgs</run_depend>

  <export>
    <rtt_ros>
      <plugin_depend>rtt_std_msgs</plugin_depend>
    </rtt_ros>
  </export>

</package>
```

`src/component.hpp`:

```cpp
#ifndef COMPONENT_HPP
#define COMPONENT_HPP

#include <std_msgs/Float64.h>

#include <rtcf/macros.hpp>
OROCOS_HEADERS_BEGIN
#include <rtt/Port.hpp>
#include <rtt/RTT.hpp>
OROCOS_HEADERS_END

class SimpleComponent : public RTT::TaskContext {
  public:
    SimpleComponent(std::string const& name);
    bool configureHook();
    bool startHook();
    void updateHook();
    void stopHook();
    void cleanupHook();

  private:
    RTT::OutputPort<std_msgs::Float64> port_out_;
    RTT::InputPort<std_msgs::Float64> port_in_;
    std_msgs::Float64 msg_;
};
#endif

```

`src/component.cpp`:

```cpp
#include "component.hpp"

#include <rtcf/macros.hpp>
OROCOS_HEADERS_BEGIN
#include <rtt/Component.hpp>
OROCOS_HEADERS_END

#include <rtcf/rt_logging_macros.hpp>

SimpleComponent::SimpleComponent(std::string const& name) : TaskContext(name), port_out_("out_port"), port_in_("in_port") {}

bool SimpleComponent::configureHook() {
    this->ports()->addPort(port_in_);
    this->ports()->addPort(port_out_);
    return true;
}

bool SimpleComponent::startHook() { return true; }

void SimpleComponent::updateHook() {
    // just forward the message
    if (port_in_.read(msg_) == RTT::NewData) {
        port_out_.write(msg_);
    }
}

void SimpleComponent::stopHook() {}

void SimpleComponent::cleanupHook() {}

ORO_CREATE_COMPONENT(SimpleComponent)

```

This component simply reads a message and outputs it again. To accomplish that, there are callbacks (`configureHook()`, `startHook()`, `updateHook()`, `stopHook()`, and `cleanupHook()`), which are called during the component life-cycle. The most important one is the `updateHook()` as this is supposed to do the heavy lifting in each iteration. Note, that `updateHook()` is called in every controller iteration independent of whether there is new data or not.

### Deploying

In the previous chapter, the creation of a simple component was presented. Now, these components need to be loaded at runtime. Here, The RTCF follows an approach very similar to *nodelets* from ROS, where multiple components (nodelets) are loaded into a single process. In contrast to nodelets, the RTCF goes further and executes all components in a single thread. This allows almost zero overhead and very high controller frequencies. The necessary resolution of dependencies arising from data exchange between components is done automatically by the RTCF as far as possible.

The launch-file for the previous example looks like this:

```xml
<launch>
    <node name="rt_runner" pkg="rtcf" type="rt_runner" output="screen">
        <rosparam param="mode">"wait_for_components"</rosparam>
        <rosparam param="ros_mapping_blacklist">".*tmp.*"</rosparam>
        <rosparam param="ros_mapping_whitelist">".*mapped.*"</rosparam>
        <rosparam param="num_components_expected">2</rosparam>
        <rosparam param="frequency">1.0</rosparam>
    </node>
    <node name="simple1" pkg="rtcf" type="rt_launcher" args="simple_component SimpleComponent" output="screen">
        <remap from="out_port" to="/mapped/tmp"/>
        <remap from="in_port" to="/mapped/in"/>
        <rosparam param="is_first">True</rosparam>
    </node>
    <node name="simple2" pkg="rtcf" type="rt_launcher" args="simple_component SimpleComponent" output="screen">
        <remap from="out_port" to="/mapped/out"/>
        <remap from="in_port" to="/mapped/tmp"/>
        <rosparam param="topics_ignore_for_graph">".*something"</rosparam>
    </node>
</launch>
```

The first `node` tag creates a so-called *rt_runner*, which is responsible for executing and organizing all components. It has several settings that can be given as parameters:

- `frequency`: The control-loop frequency in Hz.
- `ros_mapping_whitelist`: A regular expression for connections between components that shall be linked to a ROS topic with the same name.
- `ros_mapping_blacklist`: A regular expression for connections between components that shall be excluded from linking to ROS topics.
- `mode`: The mode of the runner:
  - `wait_for_components`: Do not start the control-loop until the expected number of components in `num_components_expected` is reached.
  - `no_wait`: Start the control loop immediately after a component is loaded. When additional components are loaded, the control loop is paused briefly.
  - `wait_for_trigger`: The control loop does not start and stop automatically, but it is triggered using the services provided under `/rt_runner/activate_rt_loop` and `/rt_runner/deactivate_rt_loop`.

The other `node` tags of type `rt_launcher` create an instance of the component `SimpleComponent`from the package `simple_component`. Therefore, the `rt_launcher` program, which has the following options, is used:

- `topics_ignore_for_graph`: All topics (after remapping) that are ignored for connection dependency resolution. This can contain a regular expression.
- `is_first`: Mark a component to be executed first. This is useful for components that talk to the hardware.

Note, that the main difference to a standard launch-file is the necessity for the `rt_runner`-node and the shift of the information given in the `pkg`- and `type`-attributes towards the `arg`-attribute. All other launch-file features, such as parameters, remaps, and namespaces, stay intact and are transparently handled by the RTCF.

### Real-Time Safe Code

Writing real-time safe code is not black magic, but it requires some discipline. Here are some things that should NEVER be done in a real-time code section:

- Usual logging or anything that directly prints to the console (`std::cout`, `ROS_INFO` and friends).
- Writing/reading files.
- Allocations. This includes data types where the allocation is not obvious. Pre-allocations, before real-timeness is required, are fine.
  - `std::string()`
  - `std::vector()`
  - `std::map()`
  - `new`
  - ROS message types with variable field size
  - ...

Note, that the attribute real-time safe does not necessarily imply efficient or fast. It just means that the code execution does not block for an indefinite time because it is waiting for something else.

### Advanced Features

The RTCF brings ships with many more useful features.

#### Custom Message Packages

Custom ROS message packages can be easily used within the RTCF, thanks to [rtt_roscomm](https://github.com/orocos/rtt_ros_integration/tree/toolchain-2.9/rtt_roscomm#making-a-packages-ros-msg-and-srv-types-available).

To generate such an RTCF-compatible message package, execute the following command in your workspace's `src`-directory:

```bash
rosrun rtt_roscomm create_rtt_msgs your_custom_msgs
```

To use these message in your component, add following lines to your `package.xml`:

```xml
  <run_depend>your_custom_msgs</run_depend>
  <build_depend>your_custom_msgs</build_depend>
  <export>
    <rtt_ros>
      <plugin_depend>rtt_your_custom_msgs</plugin_depend>
    </rtt_ros>
  </export>
```

Only use a single `<export>` and `<rtt_ros>` tag, if multiple message packages are required.
In your `CMakeLists.txt` add `rtt_your_custom_msgs` to your `find_package()`-command. For a working example, refer to the [rtcf_examples_msgs](rtcf_examples/), [rtt_rtcf_example_msgs](rtt_rtcf_example_msgs/), and the [Identity-component in rtcf_examples](rtcf_examples/src/identity/).

#### Parameter Handling

ROS parameters can be used in a RTCF component using the same API as in ROS nodes. Therefore, the component class must include `rtcf/rtcf_extension.hpp` and inherit from `RtcfExtension`. Then, the methods `getPrivateNodeHandle()` and `getNodeHandle()` can be used to obtain a private node handle or a node handle in the node's namespace. Refer to [rtcf_examples/src/parameter_handling/](rtcf_examples/src/parameter_handling/) for an example.

Do NOT use this in `updateHook()` as it will break real-timeness. If reading in parameters during showtime is really desired, refer to the next section.

#### Dynamic Reconfigure

The RTCF wraps *dynamic_reconfigure* to allow components that can be adjusted during runtime, e.g. for dynamically tuning controller parameters. To implement this in a custom component, a [dynamic_reconfigure-config-file](http://wiki.ros.org/dynamic_reconfigure/Tutorials) must be created and a struct with a conversion function must be provided. The struct has the following form:

```cpp
struct ExampleParameters {
    using ConfigType = rtcf_examples::ExampleConfig;
    // list all fields of the .cfg-file
    int int_param;
    double double_param;
    bool bool_param;
    int size;

    // converting constructor
    ExampleParameters(const ConfigType& config) :
        int_param(config.int_param),
        double_param(config.double_param),
        bool_param(config.bool_param),
        size(config.size) {}
    // standard constructor
    ExampleParameters() {}
};
```

If the header `rtcf/rt_dynamic_reconfigure.hpp` is included and the component class has a member such as `RTDynamicReconfigure<ExampleParameters> dynamic_config;`, the dynamic_reconfigure backend is set up with `dynamic_config.configure(getPrivateNodeHandle());` in the `configureHook()`. After that, the current parameters can be queried real-time safe using:

```cpp
ExampleParameters config_data;
dynamic_config.getValue(config_data);
```

A complete example is given in [rtcf_examples/src/parameter_handling/](rtcf_examples/src/parameter_handling/).

#### Logging

The RTCF provides two types of logging statements in [rt_logging_macros.hpp](rtcf/include/rtcf/rt_logging_macros.hpp). The use of `ROS_*`-style logging is discouraged as it breaks `rqt_logger_level` as well as real-timeness. For logging in non-real-time sections, following macros can be used:

```cpp
NON_RT_DEBUG(...);
NON_RT_DEBUG_STREAM(...);
NON_RT_INFO(...);
NON_RT_INFO_STREAM(...);
NON_RT_WARN(...);
NON_RT_WARN_STREAM(...);
NON_RT_ERROR(...);
NON_RT_ERROR_STREAM(...);
NON_RT_FATAL(...);
NON_RT_FATAL_STREAM(...);
```

For logging in real-time sections, following macros can be used:

```cpp
RT_DEBUG(...);
RT_DEBUG_STREAM(...);
RT_INFO(...);
RT_INFO_STREAM(...);
RT_WARN(...);
RT_WARN_STREAM(...);
RT_ERROR(...);
RT_ERROR_STREAM(...);
RT_FATAL(...);
RT_FATAL_STREAM(...);
```

Note, this only means that your code will still have deterministic execution times, but there is still a significant overhead for logging. Furthermore, real-time log statements can be dropped if necessary resources are not available when the logging statement is called.

Both logging styles are fully compatible with *rosconsole*, including *rqt_logger_level*, `/rosout` ,as well as `ROSCONSOLE_CONFIG_FILE`. For a complete example, refer to [rtcf_examples/src/logging/](rtcf_examples/src/logging/)

#### Runtime Analysis

To monitor the performance of the controllers, it is vital to measure the calculation time as well as the jitter of the `updateHook()`-invocations. For this reason, the *rt_runner* publishes a timestamp of the update invocation time as well as the calculation time on the topic `/rt_runner/iteration_info` after the completion of every iteration.

Additionally, some timing statistics are printed to the console each time the control loop is stopped.

#### Miscellaneous 

Some controllers might be interested in information about the current execution cycle (e.g., for stamping messages or for scaling the gains of a PID controller). To serve these needs, the component-class can inherit from the `RtcfExtension`-class, which is included using `rtcf/rtcf_extension.hpp`. This gives access to the following methods:

```cpp
this->getTime();
this->getFrequency();
this->getPeriod();
```

#### Usage in Simulations

Code that can be used in simulation as well as on real hardware is always preferred. For this reason, the RTCF implements the `/use_sim_time`-parameter that is used in ROS.

Normally, the RTCF uses `CLOCK_MONOTONIC` to perform its wait and timing operations. If `/use_sim_time` is set to true, the `/clock`-topic is used as a time source.

Due to the implementation of the underlying `rtt_rosclock`-library, care must be taken, when choosing the clock frequency: The control loop will only wake up when the control period has expired and a new clock tick arrives. This means a clock that runs slightly slower than the control frequency will result in a slightly reduced control frequency. However, a clock that runs slightly faster than the control frequency will almost half the resulting control frequency! For this reason, it is ideal if the clock frequency is much higher than the control frequency.

## Contributing

Contributions are what makes the open source community such an amazing place to learn, inspire, and create. Any contributions you make are **greatly appreciated**.

1. Fork the Project
1. Create your Feature Branch (`git checkout -b feature/AmazingFeature`)
1. Commit your Changes (`git commit -m 'Add some AmazingFeature'`)
1. Push to the Branch (`git push origin feature/AmazingFeature`)
1. Open a Pull Request

Also, feel free to open an issue at any time.

## License

Distributed under the MIT License. See [LICENSE](LICENSE) for more information.

## Contact

Michael Fennel - michael.fennel@kit.edu
Caplett - git@caplett.com

Project Link: [https://github.com/KIT-ISAS/RTCF](https://github.com/KIT-ISAS/RTCF)

## Acknowledgements

- Funded by: [Intelligent Sensor-Actuator-Systems Laboratory (ISAS)](https://isas.iar.kit.edu/), Karlsruhe Institue of Technology (KIT)
- OROCOS: [https://docs.orocos.org/](https://docs.orocos.org/)
- rtt_ros_integration: [https://github.com/orocos/rtt_ros_integration](https://github.com/orocos/rtt_ros_integration)
- ROS: [https://www.ros.org/](https://www.ros.org/)
- Readme Template: [https://github.com/othneildrew/Best-README-Template](https://github.com/othneildrew/Best-README-Template)
