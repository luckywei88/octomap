/*
 * Copyright (c) 2010-2013, A. Hornung, University of Freiburg
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the University of Freiburg nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef OCTOMAP_SERVER_OCTOMAPSERVER_H
#define OCTOMAP_SERVER_OCTOMAPSERVER_H

#include <ros/ros.h>
#include <visualization_msgs/MarkerArray.h>
#include <nav_msgs/OccupancyGrid.h>
#include <std_msgs/ColorRGBA.h>
#include <std_msgs/String.h>

// #include <moveit_msgs/CollisionObject.h>
// #include <moveit_msgs/CollisionMap.h>
#include <sensor_msgs/PointCloud2.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/CameraInfo.h>
#include <message_filters/subscriber.h>
#include <message_filters/synchronizer.h>
#include <message_filters/sync_policies/approximate_time.h>

#include <std_srvs/Empty.h>
#include <dynamic_reconfigure/server.h>
#include <octomap_server/OctomapServerConfig.h>

#include <pcl/point_types.h>
#include <pcl/conversions.h>
#include <pcl_ros/transforms.h>
#include <pcl/sample_consensus/method_types.h>
#include <pcl/sample_consensus/model_types.h>
#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/io/pcd_io.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/filters/passthrough.h>
#include <pcl_conversions/pcl_conversions.h>

#include <tf/transform_broadcaster.h>
#include <tf/transform_listener.h>
#include <tf/message_filter.h>
#include <message_filters/subscriber.h>
#include <octomap_msgs/Octomap.h>
#include <octomap_msgs/GetOctomap.h>
#include <octomap_msgs/BoundingBoxQuery.h>
#include <octomap_msgs/conversions.h>

#include <octomap_ros/conversions.h>
#include "octomap/octomap.h"
#include "octomap/OcTreeKey.h"
#include "octomap/octomap_types.h"
#include "octomap/math/Vector3.h"
#include "TimeTracker.h"

//#define COLOR_OCTOMAP_SERVER // turned off here, turned on identical ColorOctomapServer.h - easier maintenance, only maintain OctomapServer and then copy and paste to ColorOctomapServer and change define. There are prettier ways to do this, but this works for now

#ifdef COLOR_OCTOMAP_SERVER
#include <octomap/ColorOcTree.h>
#endif

using namespace octomap;

namespace octomap_server {
class OctomapServer {

public:
#ifdef COLOR_OCTOMAP_SERVER
  typedef pcl::PointXYZRGB PCLPoint;
  typedef pcl::PointCloud<pcl::PointXYZRGB> PCLPointCloud;
  typedef octomap::ColorOcTree OcTreeT;
#else
  typedef pcl::PointXYZ PCLPoint;
  typedef pcl::PointCloud<pcl::PointXYZ> PCLPointCloud;
  typedef octomap::OcTree OcTreeT;
#endif
  typedef octomap_msgs::GetOctomap OctomapSrv;
  typedef octomap_msgs::BoundingBoxQuery BBXSrv;

  OctomapServer(ros::NodeHandle private_nh_ = ros::NodeHandle("~"));
  virtual ~OctomapServer();
  virtual bool octomapBinarySrv(OctomapSrv::Request  &req, OctomapSrv::GetOctomap::Response &res);
  virtual bool octomapFullSrv(OctomapSrv::Request  &req, OctomapSrv::GetOctomap::Response &res);
  bool clearBBXSrv(BBXSrv::Request& req, BBXSrv::Response& resp);
  bool resetSrv(std_srvs::Empty::Request& req, std_srvs::Empty::Response& resp);

  virtual void insertCloudCallback(const sensor_msgs::PointCloud2::ConstPtr& cloud);
  virtual bool openFile(const std::string& filename);
  
	// add by lucky
   void msgcallback(
		const	sensor_msgs::ImageConstPtr& rgb,
		const	sensor_msgs::ImageConstPtr& dep,
		const	sensor_msgs::CameraInfoConstPtr& info,
		const	sensor_msgs::PointCloud2ConstPtr& pointcloud);
  virtual void setbyhuman(const std_msgs::String::ConstPtr& msg);
  virtual void savemap(const std_msgs::String::ConstPtr& filename);
  virtual void InitRobot(std::string filename);
  virtual void SendRobot(const ros::Time& rostime);
  

protected:
  inline static void updateMinKey(const octomap::OcTreeKey& in, octomap::OcTreeKey& min) {
    for (unsigned i = 0; i < 3; ++i)
      min[i] = std::min(in[i], min[i]);
  };

  inline static void updateMaxKey(const octomap::OcTreeKey& in, octomap::OcTreeKey& max) {
    for (unsigned i = 0; i < 3; ++i)
      max[i] = std::max(in[i], max[i]);
  };

  /// Test if key is within update area of map (2D, ignores height)
  inline bool isInUpdateBBX(const OcTreeT::iterator& it) const {
    // 2^(tree_depth-depth) voxels wide:
    unsigned voxelWidth = (1 << (m_maxTreeDepth - it.getDepth()));
    octomap::OcTreeKey key = it.getIndexKey(); // lower corner of voxel
    return (key[0] + voxelWidth >= m_updateBBXMin[0]
            && key[1] + voxelWidth >= m_updateBBXMin[1]
            && key[0] <= m_updateBBXMax[0]
            && key[1] <= m_updateBBXMax[1]);
  }

  void reconfigureCallback(octomap_server::OctomapServerConfig& config, uint32_t level);
  void publishBinaryOctoMap(const ros::Time& rostime = ros::Time::now()) const;
  void publishFullOctoMap(const ros::Time& rostime = ros::Time::now()) const;
  void publishAll(const ros::Time& rostime = ros::Time::now());
  void setOrigin(octomap_msgs::Pos& pos);
  void setend(octomap_msgs::Pos& pos);
  void delOrigin();
  void delend();
  void loopcmd(const std_msgs::String::ConstPtr& msg);
  /**
  * @brief update occupancy map with a scan labeled as ground and nonground.
  * The scans should be in the global map frame.
  *
  * @param sensorOrigin origin of the measurements for raycasting
  * @param ground scan endpoints on the ground plane (only clear space)
  * @param nonground all other endpoints (clear up to occupied endpoint)
  */
  virtual void insertScan(const tf::Point& sensorOrigin, const PCLPointCloud& ground, const PCLPointCloud& nonground);

  /// label the input cloud "pc" into ground and nonground. Should be in the robot's fixed frame (not world!)
  void filterGroundPlane(const PCLPointCloud& pc, PCLPointCloud& ground, PCLPointCloud& nonground) const;

  /**
  * @brief Find speckle nodes (single occupied voxels with no neighbors). Only works on lowest resolution!
  * @param key
  * @return
  */
  bool isSpeckleNode(const octomap::OcTreeKey& key) const;

  /// hook that is called before traversing all nodes
  virtual void handlePreNodeTraversal(const ros::Time& rostime);

  /// hook that is called when traversing all nodes of the updated Octree (does nothing here)
  virtual void handleNode(const OcTreeT::iterator& it) {};

  /// hook that is called when traversing all nodes of the updated Octree in the updated area (does nothing here)
  virtual void handleNodeInBBX(const OcTreeT::iterator& it) {};

  /// hook that is called when traversing occupied nodes of the updated Octree
  virtual void handleOccupiedNode(const OcTreeT::iterator& it);

  /// hook that is called when traversing occupied nodes in the updated area (updates 2D map projection here)
  virtual void handleOccupiedNodeInBBX(const OcTreeT::iterator& it);

  /// hook that is called when traversing free nodes of the updated Octree
  virtual void handleFreeNode(const OcTreeT::iterator& it);

  /// hook that is called when traversing free nodes in the updated area (updates 2D map projection here)
  virtual void handleFreeNodeInBBX(const OcTreeT::iterator& it);

  /// hook that is called after traversing all nodes
  virtual void handlePostNodeTraversal(const ros::Time& rostime);

  /// updates the downprojected 2D map as either occupied or free
  virtual void update2DMap(const OcTreeT::iterator& it, bool occupied);
  virtual void update2DMap(const octomap::upKey::iterator& it, bool occupied);
  virtual void clearupdatemap(int maxx,int maxy,int minx,int miny);

  inline unsigned mapIdx(int i, int j) const {
//	ROS_ERROR("i %d j %d width %d",i,j,m_gridmap.info.width);
    return m_gridmap.info.width * j + i;
  }

  inline unsigned mapIdx(const octomap::OcTreeKey& key) const {
    return mapIdx((key[0] - m_paddedMinKey[0]) / m_multires2DScale,
                  (key[1] - m_paddedMinKey[1]) / m_multires2DScale);

  }

  /**
   * Adjust data of map due to a change in its info properties (origin or size,
   * resolution needs to stay fixed). map already contains the new map info,
   * but the data is stored according to oldMapInfo.
   */

  void adjustMapData(nav_msgs::OccupancyGrid& map, const nav_msgs::MapMetaData& oldMapInfo) const;

  inline bool mapChanged(const nav_msgs::MapMetaData& oldMapInfo, const nav_msgs::MapMetaData& newMapInfo) {
    return (    oldMapInfo.height != newMapInfo.height
                || oldMapInfo.width != newMapInfo.width
                || oldMapInfo.origin.position.x != newMapInfo.origin.position.x
                || oldMapInfo.origin.position.y != newMapInfo.origin.position.y);
  }

  static std_msgs::ColorRGBA heightMapColor(double h);
  ros::NodeHandle m_nh;
  ros::Publisher  m_robotPub,m_markerPub, m_binaryMapPub, m_fullMapPub, m_pointCloudPub, m_collisionObjectPub, m_mapPub, m_cmapPub, m_fmapPub, m_fmarkerPub;
  message_filters::Subscriber<sensor_msgs::PointCloud2>* m_pointCloudSub;


	//add by lucky
  typedef message_filters::sync_policies::ApproximateTime<sensor_msgs::Image,
                                                        sensor_msgs::Image,
							sensor_msgs::CameraInfo,
                                                        sensor_msgs::PointCloud2> Sync;
  message_filters::Synchronizer<Sync>* sync_;
  message_filters::Subscriber<sensor_msgs::Image>* m_rgbSub;
  message_filters::Subscriber<sensor_msgs::Image>* m_depSub;
  message_filters::Subscriber<sensor_msgs::CameraInfo>* m_infoSub;

  ros::Publisher rgbpub,deppub,infopub,octreepub,pospub,cmdpub;
  tf::TransformBroadcaster br;
  bool loop;


;

  


  tf::MessageFilter<sensor_msgs::PointCloud2>* m_tfPointCloudSub;
  ros::ServiceServer m_octomapBinaryService, m_octomapFullService, m_clearBBXService, m_resetService;
  tf::TransformListener m_tfListener;
  ros::Subscriber setanend;
  ros::Subscriber getfile;
  ros::Subscriber cmdsub;
  dynamic_reconfigure::Server<OctomapServerConfig> m_reconfigureServer;

  OcTreeT* m_octree;
  OcTreeT* robot_octree;
  octomap::KeyRay m_keyRay;  // temp storage for ray casting
  octomap::OcTreeKey m_updateBBXMin;
  octomap::OcTreeKey m_updateBBXMax;

  double m_maxRange;
  int num;
  int robot_id;
  double m_robot_h,m_height;
  int model;
  bool m_dynamicmap,m_completemap;
  std::string m_worldFrameId; // the map frame
  std::string m_baseFrameId; // base of the robot for ground plane filtering
  std::string initfile;
  std::string m_torsoFrameId;
  bool m_useHeightMap;
  std_msgs::ColorRGBA m_color;
  std_msgs::ColorRGBA m_colorFree;
  double m_colorFactor;

  bool m_latchedTopics;
  bool m_publishFreeSpace;

  double m_res;
  unsigned m_treeDepth;
  unsigned m_maxTreeDepth;
  octomap::point3d sensorOrigin;
  PCLPointCloud torsoPC;
  float anend[3];

  double m_pointcloudMinZ;
  double m_pointcloudMaxZ;
  double m_occupancyMinZ;
  double m_occupancyMaxZ;
  double m_minSizeX;
  double m_minSizeY;
  bool m_filterSpeckles;

  bool m_filterGroundPlane;
  double m_groundFilterDistance;
  double m_groundFilterAngle;
  double m_groundFilterPlaneDistance;

  bool m_compressMap;

  TimeTracker tm,tm1;
  // downprojected 2D map:
  bool m_incrementalUpdate;
  nav_msgs::OccupancyGrid m_gridmap;
  bool m_publish2DMap;
  bool m_mapOriginChanged;
  octomap::OcTreeKey m_paddedMinKey;
  unsigned m_multires2DScale;
  bool m_projectCompleteMap;
  bool m_useColoredMap;

  double yaw;	
  int box;
};
}

#endif
