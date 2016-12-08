// Copyright 2016 Open Source Robotics Foundation, Inc.
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

#ifndef RCLCPP_LIFECYCLE__LIFECYCLE_NODE_IMPL_HPP_
#define RCLCPP_LIFECYCLE__LIFECYCLE_NODE_IMPL_HPP_

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <functional>

#include <rcl/error_handling.h>

#include <rcl_lifecycle/rcl_lifecycle.h>

#include <lifecycle_msgs/msg/transition_event.hpp>
#include <lifecycle_msgs/msg/transition_event.h>  // for getting the c-typesupport
#include <lifecycle_msgs/srv/get_state.hpp>
#include <lifecycle_msgs/srv/change_state.hpp>

#include <rclcpp/node.hpp>

#include "rclcpp_lifecycle/lifecycle_node.hpp"

namespace rclcpp
{
namespace lifecycle
{

class LifecycleNode::LifecycleNodeImpl
{

  using TransitionEventMsg = lifecycle_msgs::msg::TransitionEvent;
  using GetStateSrv = lifecycle_msgs::srv::GetState;
  using ChangeStateSrv = lifecycle_msgs::srv::ChangeState;

public:
  LifecycleNodeImpl(std::shared_ptr<rclcpp::node::Node>(base_node_handle))
  : base_node_handle_(base_node_handle)
  {}

  ~LifecycleNodeImpl()
  {
    if (rcl_lifecycle_state_machine_is_initialized(&state_machine_) != RCL_RET_OK) {
      fprintf(stderr, "%s:%u, FATAL: rcl_state_machine got destroyed externally.\n",
        __FILE__, __LINE__);
    } else {
      rcl_lifecycle_state_machine_fini(&state_machine_, base_node_handle_->get_rcl_node_handle());
    }
  }

  void
  init()
  {
    state_machine_ = rcl_lifecycle_get_zero_initialized_state_machine();
    rcl_ret_t ret = rcl_lifecycle_state_machine_init(
      &state_machine_, base_node_handle_->get_rcl_node_handle(),
      ROSIDL_GET_TYPE_SUPPORT(lifecycle_msgs, msg, TransitionEvent),
      rosidl_generator_cpp::get_service_type_support_handle<GetStateSrv>(),
      rosidl_generator_cpp::get_service_type_support_handle<ChangeStateSrv>(),
      true);
    if (ret != RCL_RET_OK) {
      fprintf(stderr, "Error adding %s: %s\n",
        base_node_handle_->get_name().c_str(), rcl_get_error_string_safe());
      return;
    }

    // srv objects may get destroyed directly here
    {  // get_state
      auto cb = std::bind(&LifecycleNodeImpl::on_get_state, this,
          std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
      rclcpp::any_service_callback::AnyServiceCallback<GetStateSrv> any_cb;
      any_cb.set(std::move(cb));

      srv_get_state_ = std::make_shared<rclcpp::service::Service<GetStateSrv>>(
        base_node_handle_->get_shared_node_handle(), &state_machine_.com_interface.srv_get_state,
        any_cb);
      base_node_handle_->add_service(
        std::dynamic_pointer_cast<service::ServiceBase>(srv_get_state_));
    }

    {  // change_state
      auto cb = std::bind(&LifecycleNodeImpl::on_change_state, this,
          std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
      rclcpp::any_service_callback::AnyServiceCallback<ChangeStateSrv> any_cb;
      any_cb.set(cb);

      srv_change_state_ = std::make_shared<rclcpp::service::Service<ChangeStateSrv>>(
        base_node_handle_->get_shared_node_handle(),
        &state_machine_.com_interface.srv_change_state,
        any_cb);
      base_node_handle_->add_service(
        std::dynamic_pointer_cast<service::ServiceBase>(srv_change_state_));
    }
  }

  bool
  register_callback(std::uint8_t lifecycle_transition, std::function<bool(void)> & cb)
  {
    cb_map_[lifecycle_transition] = cb;
    return true;
  }

  void
  on_change_state(const std::shared_ptr<rmw_request_id_t> header,
    const std::shared_ptr<lifecycle_msgs::srv::ChangeState::Request> req,
    std::shared_ptr<lifecycle_msgs::srv::ChangeState::Response> resp)
  {
    (void)header;
    if (rcl_lifecycle_state_machine_is_initialized(&state_machine_) != RCL_RET_OK) {
      resp->success = false;
      return;
    }
    resp->success = change_state(req->transition.id);
  }

  void
  on_get_state(const std::shared_ptr<rmw_request_id_t> header,
    const std::shared_ptr<lifecycle_msgs::srv::GetState::Request> req,
    std::shared_ptr<lifecycle_msgs::srv::GetState::Response> resp)
  {
    (void)header;
    (void)req;
    if (rcl_lifecycle_state_machine_is_initialized(&state_machine_) != RCL_RET_OK) {
      resp->current_state = static_cast<uint8_t>(lifecycle_msgs::msg::State::PRIMARY_STATE_UNKNOWN);
      return;
    }
    resp->current_state = static_cast<uint8_t>(state_machine_.current_state->id);
  }

  const State &
  get_current_state()
  {
    current_state_ = State(state_machine_.current_state);
    return current_state_;
  }

  bool
  change_state(std::uint8_t lifecycle_transition)
  {
    if (rcl_lifecycle_state_machine_is_initialized(&state_machine_) != RCL_RET_OK) {
      fprintf(stderr, "%s:%d, Unable to change state for state machine for %s: %s \n",
        __FILE__, __LINE__, base_node_handle_->get_name().c_str(), rcl_get_error_string_safe());
      return false;
    }

    unsigned int transition_id = static_cast<unsigned int>(lifecycle_transition);
    if (rcl_lifecycle_start_transition(&state_machine_, transition_id, true, true) != RCL_RET_OK) {
      fprintf(stderr, "%s:%d, Unable to start transition %u from current state %s: %s\n",
        __FILE__, __LINE__, transition_id,
        state_machine_.current_state->label, rcl_get_error_string_safe());
      return false;
    }

    auto cb_success = execute_callback(state_machine_.current_state->id);
    if (rcl_lifecycle_start_transition(
          &state_machine_, transition_id, cb_success, true) != RCL_RET_OK)
    {
      fprintf(stderr, "Failed to finish transition %u. Current state is now: %s\n",
        transition_id, state_machine_.current_state->label);
      return false;
    }

    // error handling ?!
    if (!cb_success)
    {
      auto error_resolved = execute_callback(state_machine_.current_state->id);
      if (error_resolved) {
        // We call cleanup on the error state
        rcl_lifecycle_start_transition(
            &state_machine_, lifecycle_msgs__msg__Transition__TRANSITION_CLEANUP, true, true);
      } else {
        // We call shutdown on the error state
        rcl_lifecycle_start_transition(
            &state_machine_, lifecycle_msgs__msg__Transition__TRANSITION_SHUTDOWN, true, true);
        }
    }
    // This true holds in both cases where the actual callback
    // was successful or not, since at this point we have a valid transistion
    // to either a new primary state or error state
    return true;
  }

  bool
  execute_callback(unsigned int cb_id)
  {
    auto cb_success = true;  // in case no callback was attached, we forward directly
    auto it = cb_map_.find(state_machine_.current_state->id);
    if (it != cb_map_.end()) {
      std::function<bool(void)> callback = it->second;
      try{
        cb_success = callback();
      } catch (const std::exception & e) {
        fprintf(stderr, "Caught exception in callback for transition %d\n",
          it->first);
        cb_success = false;
      }
    } else {
      fprintf(stderr, "%s:%d, No callback is registered for transition %u.\n",
        __FILE__, __LINE__, cb_id);
    }
    return cb_success;
  }


  const State &
  trigger_transition(unsigned int transition_id)
  {
    change_state(transition_id);
    return get_current_state();
  }

  void
  add_publisher_handle(std::shared_ptr<rclcpp::lifecycle::LifecyclePublisherInterface> pub)
  {
    weak_pubs_.push_back(pub);
  }

  void
  add_timer_handle(std::shared_ptr<rclcpp::timer::TimerBase> timer)
  {
    weak_timers_.push_back(timer);
  }

  rcl_lifecycle_state_machine_t state_machine_;
  State current_state_;
  std::map<std::uint8_t, std::function<bool(void)>> cb_map_;

  std::shared_ptr<rclcpp::node::Node> base_node_handle_;
  std::shared_ptr<rclcpp::service::Service<lifecycle_msgs::srv::ChangeState>> srv_change_state_;
  std::shared_ptr<rclcpp::service::Service<lifecycle_msgs::srv::GetState>> srv_get_state_;

  // to controllable things
  std::vector<std::weak_ptr<rclcpp::lifecycle::LifecyclePublisherInterface>> weak_pubs_;
  std::vector<std::weak_ptr<rclcpp::timer::TimerBase>> weak_timers_;
};

}  // namespace lifecycle
}  // namespace rclcpp
#endif  // RCLCPP_LIFECYCLE__LIFECYCLE_MANAGER_IMPL_HPP_
