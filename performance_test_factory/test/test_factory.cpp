/* Software License Agreement (BSD License)
 *
 *  Copyright (c) 2019, iRobot ROS
 *  All rights reserved.
 *
 *  This file is part of ros2-performance, which is released under BSD-3-Clause.
 *  You may use, distribute and modify this code under the BSD-3-Clause license.
 */

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "performance_test_factory/factory.hpp"

class TestFactory : public ::testing::Test
{
public:
  static void SetUpTestCase()
  {
    rclcpp::init(0, nullptr);
  }
};

TEST_F(TestFactory, FactoryConstructorTest)
{
  performance_test_factory::TemplateFactory factory;
}

TEST_F(TestFactory, FactoryCreateFromStringTest)
{
  performance_test_factory::TemplateFactory factory;

  auto node =
    std::make_shared<performance_test::PerformanceNode<rclcpp::Node>>("node_name");

  factory.add_subscriber_from_strings(
    node,
    "stamped10b",
    "my_topic",
    performance_metrics::Tracker::Options());
  factory.add_periodic_publisher_from_strings(
    node,
    "stamped10b",
    "my_topic");
  factory.add_server_from_strings(
    node,
    "stamped10b",
    "my_service");
  factory.add_periodic_client_from_strings(
    node,
    "stamped10b",
    "my_service");

  ASSERT_EQ(1u, node->sub_trackers().size());
  ASSERT_EQ(1u, node->client_trackers().size());
  ASSERT_EQ(1u, node->pub_trackers().size());
}

TEST_F(TestFactory, FactoryCreateFromIndicesTest)
{
  performance_test_factory::TemplateFactory factory;

  int n_subscriber_nodes = 2;
  int n_publisher_nodes = 2;
  std::string msg_type = "stamped10b";
  float frequency = 1;

  int subscriber_start_index = 0;
  int subscriber_end_index = n_subscriber_nodes;
  int publisher_start_index = n_subscriber_nodes;
  int publisher_end_index = n_subscriber_nodes + n_publisher_nodes;

  auto sub_nodes = factory.create_subscriber_nodes(
    subscriber_start_index,
    subscriber_end_index,
    n_publisher_nodes,
    msg_type,
    PASS_BY_SHARED_PTR,
    performance_metrics::Tracker::Options());

  auto pub_nodes = factory.create_periodic_publisher_nodes(
    publisher_start_index,
    publisher_end_index,
    frequency,
    msg_type,
    PASS_BY_UNIQUE_PTR);

  ASSERT_EQ(static_cast<size_t>(2), sub_nodes.size());
  ASSERT_EQ(static_cast<size_t>(2), pub_nodes.size());

  for (const auto & n : sub_nodes) {
    ASSERT_EQ(2u, n->sub_trackers().size());
    ASSERT_EQ(0u, n->client_trackers().size());
    ASSERT_EQ(0u, n->pub_trackers().size());
  }
  for (const auto & n : pub_nodes) {
    ASSERT_EQ(0u, n->sub_trackers().size());
    ASSERT_EQ(0u, n->client_trackers().size());
    ASSERT_EQ(1u, n->pub_trackers().size());
  }
}

TEST_F(TestFactory, FactoryCreateFromJsonTest)
{
  std::string this_file_path = __FILE__;
  std::string this_dir_path = this_file_path.substr(0, this_file_path.rfind("/"));
  std::string json_path = this_dir_path + std::string("/files/test_architecture.json");

  performance_test_factory::TemplateFactory factory;

  auto nodes_vec = factory.parse_topology_from_json(
    json_path,
    performance_metrics::Tracker::Options());

  ASSERT_EQ(static_cast<size_t>(3), nodes_vec.size());

  ASSERT_STREQ("node_0", nodes_vec[0]->get_node_name());
  ASSERT_STREQ("node_1", nodes_vec[1]->get_node_name());
  ASSERT_STREQ("node_2", nodes_vec[2]->get_node_name());

  ASSERT_EQ(0u, nodes_vec[0]->sub_trackers().size());
  ASSERT_EQ(0u, nodes_vec[0]->client_trackers().size());
  ASSERT_EQ(2u, nodes_vec[0]->pub_trackers().size());

  ASSERT_EQ(1u, nodes_vec[1]->sub_trackers().size());
  ASSERT_EQ(0u, nodes_vec[1]->client_trackers().size());
  ASSERT_EQ(0u, nodes_vec[1]->pub_trackers().size());

  ASSERT_EQ(0u, nodes_vec[2]->sub_trackers().size());
  ASSERT_EQ(1u, nodes_vec[2]->client_trackers().size());
  ASSERT_EQ(0u, nodes_vec[2]->pub_trackers().size());
}
