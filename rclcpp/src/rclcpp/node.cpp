// Copyright 2015 Open Source Robotics Foundation, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "rclcpp/exceptions.hpp"
#include "rclcpp/graph_listener.hpp"
#include "rclcpp/node.hpp"
#include "rclcpp/node_interfaces/node_base.hpp"
#include "rclcpp/node_interfaces/node_graph.hpp"
#include "rclcpp/node_interfaces/node_parameters.hpp"
#include "rclcpp/node_interfaces/node_services.hpp"
#include "rclcpp/node_interfaces/node_timers.hpp"
#include "rclcpp/node_interfaces/node_topics.hpp"

using rclcpp::node::Node;
using rclcpp::exceptions::throw_from_rcl_error;

Node::Node(
  const std::string & node_name,
  const std::string & namespace_,
  bool use_intra_process_comms)
: Node(
    node_name,
    namespace_,
    rclcpp::contexts::default_context::get_global_default_context(),
    use_intra_process_comms)
{}

Node::Node(
  const std::string & node_name,
  const std::string & namespace_,
  rclcpp::context::Context::SharedPtr context,
  bool use_intra_process_comms)
: node_base_(new rclcpp::node_interfaces::NodeBase(node_name, namespace_, context)),
  node_graph_(new rclcpp::node_interfaces::NodeGraph(node_base_.get())),
  node_timers_(new rclcpp::node_interfaces::NodeTimers(node_base_.get())),
  node_topics_(new rclcpp::node_interfaces::NodeTopics(node_base_.get())),
  node_services_(new rclcpp::node_interfaces::NodeServices(node_base_.get())),
  node_parameters_(new rclcpp::node_interfaces::NodeParameters(
      node_topics_.get(),
      use_intra_process_comms
    )),
  use_intra_process_comms_(use_intra_process_comms)
{
}

Node::~Node()
{}

const char *
Node::get_name() const
{
  return node_base_->get_name();
}

const char *
Node::get_namespace() const
{
  return node_base_->get_namespace();
}

rclcpp::callback_group::CallbackGroup::SharedPtr
Node::create_callback_group(
  rclcpp::callback_group::CallbackGroupType group_type)
{
  return node_base_->create_callback_group(group_type);
}

bool
Node::group_in_node(rclcpp::callback_group::CallbackGroup::SharedPtr group)
{
  return node_base_->callback_group_in_node(group);
}

std::vector<rcl_interfaces::msg::SetParametersResult>
Node::set_parameters(
  const std::vector<rclcpp::parameter::ParameterVariant> & parameters)
{
  return node_parameters_->set_parameters(parameters);
}

rcl_interfaces::msg::SetParametersResult
Node::set_parameters_atomically(
  const std::vector<rclcpp::parameter::ParameterVariant> & parameters)
{
  return node_parameters_->set_parameters_atomically(parameters);
}

std::vector<rclcpp::parameter::ParameterVariant>
Node::get_parameters(
  const std::vector<std::string> & names) const
{
  return node_parameters_->get_parameters(names);
}

rclcpp::parameter::ParameterVariant
Node::get_parameter(const std::string & name) const
{
  return node_parameters_->get_parameter(name);
}

bool Node::get_parameter(const std::string & name,
  rclcpp::parameter::ParameterVariant & parameter) const
{
  return node_parameters_->get_parameter(name, parameter);
}

std::vector<rcl_interfaces::msg::ParameterDescriptor>
Node::describe_parameters(
  const std::vector<std::string> & names) const
{
  return node_parameters_->describe_parameters(names);
}

std::vector<uint8_t>
Node::get_parameter_types(
  const std::vector<std::string> & names) const
{
  return node_parameters_->get_parameter_types(names);
}

rcl_interfaces::msg::ListParametersResult
Node::list_parameters(
  const std::vector<std::string> & prefixes, uint64_t depth) const
{
  return node_parameters_->list_parameters(prefixes, depth);
}

std::map<std::string, std::string>
Node::get_topic_names_and_types() const
{
  return node_graph_->get_topic_names_and_types();
}

size_t
Node::count_publishers(const std::string & topic_name) const
{
  return node_graph_->count_publishers(topic_name);
}

size_t
Node::count_subscribers(const std::string & topic_name) const
{
  return node_graph_->count_subscribers(topic_name);
}

const std::vector<rclcpp::callback_group::CallbackGroup::WeakPtr> &
Node::get_callback_groups() const
{
  return node_base_->get_callback_groups();
}

rclcpp::event::Event::SharedPtr
Node::get_graph_event()
{
  return node_graph_->get_graph_event();
}

void
Node::wait_for_graph_change(
  rclcpp::event::Event::SharedPtr event,
  std::chrono::nanoseconds timeout)
{
  node_graph_->wait_for_graph_change(event, timeout);
}

rclcpp::node_interfaces::NodeBaseInterface::SharedPtr
Node::get_node_base_interface()
{
  return node_base_;
}

rclcpp::node_interfaces::NodeGraphInterface::SharedPtr
Node::get_node_graph_interface()
{
  return node_graph_;
}

rclcpp::node_interfaces::NodeTimersInterface::SharedPtr
Node::get_node_timers_interface()
{
  return node_timers_;
}

rclcpp::node_interfaces::NodeTopicsInterface::SharedPtr
Node::get_node_topics_interface()
{
  return node_topics_;
}

rclcpp::node_interfaces::NodeServicesInterface::SharedPtr
Node::get_node_services_interface()
{
  return node_services_;
}

rclcpp::node_interfaces::NodeParametersInterface::SharedPtr
Node::get_node_parameters_interface()
{
  return node_parameters_;
}
