// Copyright 2017 Open Source Robotics Foundation, Inc.
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

#ifndef RCLCPP__TIME_HPP_
#define RCLCPP__TIME_HPP_

#include <utility>

#include "builtin_interfaces/msg/time.hpp"
#include "rcl/time.h"
#include "rcutils/time.h"

#include "rclcpp/exceptions.hpp"

namespace rclcpp
{

class Time
{
public:
  template<rcl_time_source_type_t ClockT = RCL_SYSTEM_TIME>
  static Time
  now()
  {
    rcutils_time_point_value_t rcutils_now = 0;
    rcutils_ret_t ret = RCUTILS_RET_ERROR;
    if (ClockT == RCL_ROS_TIME) {
      throw std::runtime_error("RCL_ROS_TIME is currently not implemented.");
      ret = false;
    } else if (ClockT == RCL_SYSTEM_TIME) {
      ret = rcutils_system_time_now(&rcutils_now);
    } else if (ClockT == RCL_STEADY_TIME) {
      ret = rcutils_steady_time_now(&rcutils_now);
    }
    if (ret != RCUTILS_RET_OK) {
      rclcpp::exceptions::throw_from_rcl_error(
        ret, "Could not get current time: ");
    }

    return Time(std::move(rcutils_now));
  }

  operator builtin_interfaces::msg::Time() const
  {
    builtin_interfaces::msg::Time msg_time;
    msg_time.sec = static_cast<std::int32_t>(RCL_NS_TO_S(rcl_time_));
    msg_time.nanosec = static_cast<std::uint32_t>(rcl_time_ % (1000 * 1000 * 1000));
    return msg_time;
  }

private:
  rcl_time_point_value_t rcl_time_;

  Time(std::uint32_t sec, std::uint32_t nanosec)
  : rcl_time_(RCL_S_TO_NS(sec) + nanosec)
  {}

  explicit Time(rcl_time_point_value_t && rcl_time)
  : rcl_time_(std::forward<decltype(rcl_time)>(rcl_time))
  {}
};

}  // namespace rclcpp

#endif  // RCLCPP__TIME_HPP_
