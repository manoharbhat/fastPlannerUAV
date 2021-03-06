/*  * Copyright 2015 Fadri Furrer, ASL, ETH Zurich, Switzerland  * Copyright
2015 Michael Burri, ASL, ETH Zurich, Switzerland  * Copyright 2015 Mina Kamel,
ASL, ETH Zurich, Switzerland  * Copyright 2015 Janosch Nikolic, ASL, ETH
Zurich, Switzerland  * Copyright 2015 Markus Achtelik, ASL, ETH Zurich,
Switzerland  *  * Licensed under the Apache License, Version 2.0 (the
"License");  * you may not use this file except in compliance with the
License.  * You may obtain a copy of the License at  *  *
http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "msg_check/msg_check.h"
#include <eigen_conversions/eigen_msg.h>


namespace msg_check {

  MsgCheck::MsgCheck() {
    ROS_INFO("Msg Called");
  }

  void MsgCheck::writeMsg(msg_check::PlotDataMsg* data) {
    data->thrust = 5;
  }




}


// void callback(const sensor_msgs::LaserScan::ConstPtr& input) {
  //ROS_INFO("Laser Signal Recieved");
// }

