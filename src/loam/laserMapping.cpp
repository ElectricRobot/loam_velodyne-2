#include "laserMapping.h"
namespace laserMapping
{

laserMapping::laserMapping()
{

    if (!init)
    {
        resetMap();
        init = true;
    }


    matA0 = cv::Mat(5, 3, CV_32F, cv::Scalar::all(0));
    matB0 = cv::Mat(5, 1, CV_32F, cv::Scalar::all(-1));
    matX0 = cv::Mat(3, 1, CV_32F, cv::Scalar::all(0));

    matA1 = cv::Mat(3, 3, CV_32F, cv::Scalar::all(0));
    matD1 = cv::Mat(1, 3, CV_32F, cv::Scalar::all(0));
    matV1 = cv::Mat(3, 3, CV_32F, cv::Scalar::all(0));

    laserCloudMap.reset(new pcl::PointCloud<pcl::PointXYZHSV>());
    laserCloudMapCorres.reset(new pcl::PointCloud<pcl::PointXYZHSV>());

    laserCloudLast.reset(new pcl::PointCloud<pcl::PointXYZHSV>());
    laserCloudOri.reset(new pcl::PointCloud<pcl::PointXYZHSV>());

    coeffSel.reset(new pcl::PointCloud<pcl::PointXYZHSV>());
    laserCloudSurround.reset(new pcl::PointCloud<pcl::PointXYZI>());
    //laserCloudArray[laserCloudNum];

    kdtreeCornerFromMap.reset(new pcl::KdTreeFLANN<pcl::PointXYZHSV>());
    kdtreeSurfFromMap.reset(new pcl::KdTreeFLANN<pcl::PointXYZHSV>());

    pubMapBeforeNewInput = nh.advertise<sensor_msgs::PointCloud2>("/laserMapping_MapBeforeInput", 1);
    pubMapAfterNewInput = nh.advertise<sensor_msgs::PointCloud2>("/laserMapping_MapAfterInput", 1);


}

void laserMapping::resetMap()
{
    for (int i = 0; i < laserCloudNum; i++) {
        laserCloudArray[i].reset(new pcl::PointCloud<pcl::PointXYZHSV>());
    }
}


void laserMapping::transformAssociateToMap()
{
    float x1 = cos(transformSum[1]) * (transformBefMapped[3] - transformSum[3])
            - sin(transformSum[1]) * (transformBefMapped[5] - transformSum[5]);
    float y1 = transformBefMapped[4] - transformSum[4];
    float z1 = sin(transformSum[1]) * (transformBefMapped[3] - transformSum[3])
            + cos(transformSum[1]) * (transformBefMapped[5] - transformSum[5]);

    float x2 = x1;
    float y2 = cos(transformSum[0]) * y1 + sin(transformSum[0]) * z1;
    float z2 = -sin(transformSum[0]) * y1 + cos(transformSum[0]) * z1;

    transformIncre[3] = cos(transformSum[2]) * x2 + sin(transformSum[2]) * y2;
    transformIncre[4] = -sin(transformSum[2]) * x2 + cos(transformSum[2]) * y2;
    transformIncre[5] = z2;

    float sbcx = sin(transformSum[0]);
    float cbcx = cos(transformSum[0]);
    float sbcy = sin(transformSum[1]);
    float cbcy = cos(transformSum[1]);
    float sbcz = sin(transformSum[2]);
    float cbcz = cos(transformSum[2]);

    float sblx = sin(transformBefMapped[0]);
    float cblx = cos(transformBefMapped[0]);
    float sbly = sin(transformBefMapped[1]);
    float cbly = cos(transformBefMapped[1]);
    float sblz = sin(transformBefMapped[2]);
    float cblz = cos(transformBefMapped[2]);

    float salx = sin(transformAftMapped[0]);
    float calx = cos(transformAftMapped[0]);
    float saly = sin(transformAftMapped[1]);
    float caly = cos(transformAftMapped[1]);
    float salz = sin(transformAftMapped[2]);
    float calz = cos(transformAftMapped[2]);

    float srx = -sbcx*(salx*sblx + calx*caly*cblx*cbly + calx*cblx*saly*sbly)
            - cbcx*cbcz*(calx*saly*(cbly*sblz - cblz*sblx*sbly)
                         - calx*caly*(sbly*sblz + cbly*cblz*sblx) + cblx*cblz*salx)
            - cbcx*sbcz*(calx*caly*(cblz*sbly - cbly*sblx*sblz)
                         - calx*saly*(cbly*cblz + sblx*sbly*sblz) + cblx*salx*sblz);
    transformTobeMapped[0] = -asin(srx);

    float srycrx = (cbcy*sbcz - cbcz*sbcx*sbcy)*(calx*saly*(cbly*sblz - cblz*sblx*sbly)
                                                 - calx*caly*(sbly*sblz + cbly*cblz*sblx) + cblx*cblz*salx)
            - (cbcy*cbcz + sbcx*sbcy*sbcz)*(calx*caly*(cblz*sbly - cbly*sblx*sblz)
                                            - calx*saly*(cbly*cblz + sblx*sbly*sblz) + cblx*salx*sblz)
            + cbcx*sbcy*(salx*sblx + calx*caly*cblx*cbly + calx*cblx*saly*sbly);
    float crycrx = (cbcz*sbcy - cbcy*sbcx*sbcz)*(calx*caly*(cblz*sbly - cbly*sblx*sblz)
                                                 - calx*saly*(cbly*cblz + sblx*sbly*sblz) + cblx*salx*sblz)
            - (sbcy*sbcz + cbcy*cbcz*sbcx)*(calx*saly*(cbly*sblz - cblz*sblx*sbly)
                                            - calx*caly*(sbly*sblz + cbly*cblz*sblx) + cblx*cblz*salx)
            + cbcx*cbcy*(salx*sblx + calx*caly*cblx*cbly + calx*cblx*saly*sbly);
    transformTobeMapped[1] = atan2(srycrx / cos(transformTobeMapped[0]),
            crycrx / cos(transformTobeMapped[0]));

    float srzcrx = sbcx*(cblx*cbly*(calz*saly - caly*salx*salz)
                         - cblx*sbly*(caly*calz + salx*saly*salz) + calx*salz*sblx)
            - cbcx*cbcz*((caly*calz + salx*saly*salz)*(cbly*sblz - cblz*sblx*sbly)
                         + (calz*saly - caly*salx*salz)*(sbly*sblz + cbly*cblz*sblx)
                         - calx*cblx*cblz*salz) + cbcx*sbcz*((caly*calz + salx*saly*salz)*(cbly*cblz
                                                                                           + sblx*sbly*sblz) + (calz*saly - caly*salx*salz)*(cblz*sbly - cbly*sblx*sblz)
                                                             + calx*cblx*salz*sblz);
    float crzcrx = sbcx*(cblx*sbly*(caly*salz - calz*salx*saly)
                         - cblx*cbly*(saly*salz + caly*calz*salx) + calx*calz*sblx)
            + cbcx*cbcz*((saly*salz + caly*calz*salx)*(sbly*sblz + cbly*cblz*sblx)
                         + (caly*salz - calz*salx*saly)*(cbly*sblz - cblz*sblx*sbly)
                         + calx*calz*cblx*cblz) - cbcx*sbcz*((saly*salz + caly*calz*salx)*(cblz*sbly
                                                                                           - cbly*sblx*sblz) + (caly*salz - calz*salx*saly)*(cbly*cblz + sblx*sbly*sblz)
                                                             - calx*calz*cblx*sblz);
    transformTobeMapped[2] = atan2(srzcrx / cos(transformTobeMapped[0]),
            crzcrx / cos(transformTobeMapped[0]));

    x1 = cos(transformTobeMapped[2]) * transformIncre[3] - sin(transformTobeMapped[2]) * transformIncre[4];
    y1 = sin(transformTobeMapped[2]) * transformIncre[3] + cos(transformTobeMapped[2]) * transformIncre[4];
    z1 = transformIncre[5];

    x2 = x1;
    y2 = cos(transformTobeMapped[0]) * y1 - sin(transformTobeMapped[0]) * z1;
    z2 = sin(transformTobeMapped[0]) * y1 + cos(transformTobeMapped[0]) * z1;

    transformTobeMapped[3] = transformAftMapped[3]
            - (cos(transformTobeMapped[1]) * x2 + sin(transformTobeMapped[1]) * z2);
    transformTobeMapped[4] = transformAftMapped[4] - y2;
    transformTobeMapped[5] = transformAftMapped[5]
            - (-sin(transformTobeMapped[1]) * x2 + cos(transformTobeMapped[1]) * z2);
}

void laserMapping::transformUpdate()
{
    //for (int i = 0; i < 3; i++) {
    //  transformTobeMapped[i] = (1 - 0.1) * transformTobeMapped[i] + 0.1 * transformSum[i];
    //}

    for (int i = 0; i < 6; i++) {
        transformBefMapped[i] = transformSum[i];
        transformAftMapped[i] = transformTobeMapped[i];
    }
}

void laserMapping::pointAssociateToMap(pcl::PointXYZHSV *pi, pcl::PointXYZHSV *po)
{
    float x1 = cos(transformTobeMapped[2]) * pi->x
            - sin(transformTobeMapped[2]) * pi->y;
    float y1 = sin(transformTobeMapped[2]) * pi->x
            + cos(transformTobeMapped[2]) * pi->y;
    float z1 = pi->z;

    float x2 = x1;
    float y2 = cos(transformTobeMapped[0]) * y1 - sin(transformTobeMapped[0]) * z1;
    float z2 = sin(transformTobeMapped[0]) * y1 + cos(transformTobeMapped[0]) * z1;

    po->x = cos(transformTobeMapped[1]) * x2 + sin(transformTobeMapped[1]) * z2
            + transformTobeMapped[3];
    po->y = y2 + transformTobeMapped[4];
    po->z = -sin(transformTobeMapped[1]) * x2 + cos(transformTobeMapped[1]) * z2
            + transformTobeMapped[5];
    po->h = pi->h;
    po->s = pi->s;
    po->v = pi->v;
}

void laserMapping::pointAssociateToMapEig(pcl::PointXYZHSV *pi, pcl::PointXYZHSV *po)
{
    Eigen::Vector3f pi_;
    pi_ << pi->x, pi->y, pi->z;
    Eigen::Vector3f po_;
    Eigen::Quaternionf q;
    geometry_msgs::Quaternion geoQuat = tf::createQuaternionMsgFromRollPitchYaw(transformTobeMapped[0], transformTobeMapped[1], transformTobeMapped[2]);
    q.x() = geoQuat.x;
    q.y() = geoQuat.y;
    q.z() = geoQuat.z;
    q.w() = geoQuat.w;
    Eigen::Matrix3f R = q.matrix();
    //R = R.inverse();
    Eigen::Vector3f t;
    t << transformTobeMapped[3], transformTobeMapped[4], transformTobeMapped[5];
    //t = -R*t;
    po_ = R*pi_+t;

    po->x = po_(0);
    po->y = po_(1);
    po->z = po_(2);
    po->h = pi->h;
    po->s = pi->s;
    po->v = pi->v;
}

void laserMapping::pointAssociateToMapEigInv(pcl::PointXYZHSV *pi, pcl::PointXYZHSV *po)
{
    Eigen::Vector3f pi_;
    pi_ << pi->x, pi->y, pi->z;
    Eigen::Vector3f po_;
    Eigen::Quaternionf q;
    geometry_msgs::Quaternion geoQuat = tf::createQuaternionMsgFromRollPitchYaw(transformTobeMapped[0], transformTobeMapped[1], transformTobeMapped[2]);
    q.x() = geoQuat.x;
    q.y() = geoQuat.y;
    q.z() = geoQuat.z;
    q.w() = geoQuat.w;
    Eigen::Matrix3f R = q.matrix();
    R = R.inverse();
    Eigen::Vector3f t;
    t << transformTobeMapped[3], transformTobeMapped[4], transformTobeMapped[5];
    t = -R*t;
    po_ = R*pi_+t;

    po->x = po_(0);
    po->y = po_(1);
    po->z = po_(2);
    po->h = pi->h;
    po->s = pi->s;
    po->v = pi->v;
}

void laserMapping::laserCloudLastHandler(const sensor_msgs::PointCloud2 &laserCloudLast2)
{
    timeLaserCloudLast = laserCloudLast2.header.stamp.toSec();

    laserCloudLast->clear();
    pcl::fromROSMsg(laserCloudLast2, *laserCloudLast);

    newLaserCloudLast = true;
}

void laserMapping::laserCloudLastHandlerVelo(const pcl::PointCloud<pcl::PointXYZHSV>::Ptr inLaserCloudLast)
{
    //timeLaserCloudLast = laserCloudLast2.header.stamp.toSec();

    laserCloudLast->clear();
    //pcl::fromROSMsg(laserCloudLast2, *laserCloudLast);
    *laserCloudLast = *inLaserCloudLast;

    newLaserCloudLast = true;
}

void laserMapping::laserOdometryHandler(const nav_msgs::Odometry &laserOdometry)
{
    timeLaserOdometry = laserOdometry.header.stamp.toSec();

    double roll, pitch, yaw;
    geometry_msgs::Quaternion geoQuat = laserOdometry.pose.pose.orientation;
    tf::Matrix3x3(tf::Quaternion(geoQuat.z, -geoQuat.x, -geoQuat.y, geoQuat.w)).getRPY(roll, pitch, yaw);

    transformSum[0] = -pitch;
    transformSum[1] = -yaw;
    transformSum[2] = roll;

    transformSum[3] = laserOdometry.pose.pose.position.x;
    transformSum[4] = laserOdometry.pose.pose.position.y;
    transformSum[5] = laserOdometry.pose.pose.position.z;

    newLaserOdometry = true;
}

void laserMapping::laserOdometryHandlerVelo(const Eigen::Matrix4d T)
{
    //timeLaserOdometry = laserOdometry.header.stamp.toSec();

    double roll, pitch, yaw;
    //geometry_msgs::Quaternion geoQuat = laserOdometry.pose.pose.orientation;

    Eigen::Matrix3d R = T.topLeftCorner(3,3);
    Eigen::Quaterniond q(R);
    tf::Matrix3x3(tf::Quaternion(q.x(), q.y(), q.z(), q.w())).getRPY(roll, pitch, yaw);
    //Eigen::Vector3d euler = R.eulerAngles(2,0,2);
    transformSum[0] = pitch;
    transformSum[1] = yaw;
    transformSum[2] = roll;

    transformSum[3] = T(0,3);
    transformSum[4] = T(1,3);
    transformSum[5] = T(2,3);

    newLaserOdometry = true;
}

void laserMapping::loop()
{
    if (newLaserCloudLast && newLaserOdometry) // && fabs(timeLaserCloudLast - timeLaserOdometry) < 0.005)
    {
        newLaserCloudLast = false;
        newLaserOdometry = false;

        //transformAssociateToMap();

        ///
//        int centerCubeI = int((transformTobeMapped[3] + 10.0) / 20.0) + laserCloudCenWidth;
//        int centerCubeJ = int((transformTobeMapped[4] + 10.0) / 20.0) + laserCloudCenHeight;
//        int centerCubeK = int((transformTobeMapped[5] + 10.0) / 20.0) + laserCloudCenDepth;

//        if (transformTobeMapped[3] + 10.0 < 0) centerCubeI--;
//        if (transformTobeMapped[4] + 10.0 < 0) centerCubeJ--;
//        if (transformTobeMapped[5] + 10.0 < 0) centerCubeK--;

//        std::cout << "centerCubeI=" << centerCubeI << ", centerCubeJ=" << centerCubeJ << ", centerCubeK=" << centerCubeK << std::endl;
//        std::cout << "laserCloudWidth=" << laserCloudWidth << ", laserCloudHeight=" << laserCloudHeight << ", laserCloudDepth=" << laserCloudDepth << std::endl;

//        // centerCube indices out of bounds
//        if (centerCubeI < 0 || centerCubeI >= laserCloudWidth || centerCubeJ < 0 || centerCubeJ >= laserCloudHeight || centerCubeK < 0 || centerCubeK >= laserCloudDepth)
//        {
//            transformUpdate();
//            std::cout << "[WARNING] centerCube indices out of bounds" << std::endl;
//            return;
//        }

//        /// compute laserCloudValidInd and laserCloudSurroundInd
//        pcl::PointXYZHSV pointOnZAxis;
//        pointOnZAxis.x = 0.0;
//        pointOnZAxis.y = 0.0;
//        pointOnZAxis.z = 10.0;
//        pointAssociateToMapEig(&pointOnZAxis, &pointOnZAxis);
//        int laserCloudValidNum = 0;
//        int laserCloudSurroundNum = 0;
//        for (int i = centerCubeI - 1; i <= centerCubeI + 1; i++)
//        {
//            for (int j = centerCubeJ - 1; j <= centerCubeJ + 1; j++)
//            {
//                for (int k = centerCubeK - 1; k <= centerCubeK + 1; k++)
//                {
//                    // if centerCube indices are in bounds
//                    if (i >= 0 && i < laserCloudWidth && j >= 0 && j < laserCloudHeight && k >= 0 && k < laserCloudDepth)
//                    {
//                        float centerX = 20.0 * (i - laserCloudCenWidth);
//                        float centerY = 20.0 * (j - laserCloudCenHeight);
//                        float centerZ = 20.0 * (k - laserCloudCenDepth);

//                        bool isInLaserFOV = false;
//                        // check if is in laser field of view
//                        for (int ii = -1; ii <= 1; ii += 2)
//                        {
//                            for (int jj = -1; jj <= 1; jj += 2)
//                            {
//                                for (int kk = -1; kk <= 1; kk += 2)
//                                {
//                                    float cornerX = centerX + 10.0 * ii;
//                                    float cornerY = centerY + 10.0 * jj;
//                                    float cornerZ = centerZ + 10.0 * kk;

//                                    float check = 100.0
//                                            + (transformTobeMapped[3] - cornerX) * (transformTobeMapped[3] - cornerX)
//                                            + (transformTobeMapped[4] - cornerY) * (transformTobeMapped[4] - cornerY)
//                                            + (transformTobeMapped[5] - cornerZ) * (transformTobeMapped[5] - cornerZ)
//                                            - (pointOnZAxis.x - cornerX) * (pointOnZAxis.x - cornerX)
//                                            - (pointOnZAxis.y - cornerY) * (pointOnZAxis.y - cornerY)
//                                            - (pointOnZAxis.z - cornerZ) * (pointOnZAxis.z - cornerZ);

//                                    if (check > 0)
//                                        isInLaserFOV = true;
//                                    //else
//                                    //std::cout << "[WARNING]: is not in field of view check=" << check << std::endl;
//                                }
//                            }
//                        }

//                        if (1)
//                            laserCloudValidInd[laserCloudValidNum++] = i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k;

//                        laserCloudSurroundInd[laserCloudSurroundNum++] = i + laserCloudWidth * j + laserCloudWidth * laserCloudHeight * k;
//                    }
//                }
//            }
//        }

        pcl::PointCloud<pcl::PointXYZHSV>::Ptr laserCloudFromMap(new pcl::PointCloud<pcl::PointXYZHSV>());
        pcl::PointCloud<pcl::PointXYZHSV>::Ptr laserCloudCornerFromMap(new pcl::PointCloud<pcl::PointXYZHSV>());
        pcl::PointCloud<pcl::PointXYZHSV>::Ptr laserCloudSurfFromMap(new pcl::PointCloud<pcl::PointXYZHSV>());

        // lasercloud from cubes
//        laserCloudFromMap->clear();
//        for (int i = 0; i < laserCloudValidNum; i++)
//        {
//            *laserCloudFromMap += *laserCloudArray[laserCloudValidInd[i]];
//        }

        *laserCloudFromMap = *laserCloudMap;

        sensor_msgs::PointCloud2 pc;
        pcl::toROSMsg(*laserCloudFromMap, pc);
        pc.header.stamp = ros::Time().now();
        pc.header.frame_id = "/world";
        pubMapBeforeNewInput.publish(pc);


        // separate cloud into corners and surf points
        extractFeatures(laserCloudCornerFromMap, laserCloudSurfFromMap, laserCloudFromMap);

        downsampleInputCloud();

        doICP_new(laserCloudSurfFromMap,laserCloudCornerFromMap);

        transformUpdate();

        //resetMap();
        /// store point in array based on cube indeces
        //storeMapInCubes(laserCloudLast);

//        laserCloudFromMap->clear();
//        for (int i = 0; i < laserCloudValidNum; i++)
//        {
//            *laserCloudFromMap += *laserCloudArray[laserCloudValidInd[i]];
//        }







        pcl::PointCloud<pcl::PointXYZHSV>::Ptr transformedPC(new pcl::PointCloud<pcl::PointXYZHSV>());
        for (int i=0;i<laserCloudLast->points.size();i++)
        {
            pcl::PointXYZHSV point;
            pointAssociateToMapEig(&laserCloudLast->points[i], &point);
            transformedPC->points.push_back(point);
        }
//        pcl::VoxelGrid<pcl::PointXYZHSV> downSizeFilter;
//        downSizeFilter.setInputCloud(transformedPC);
//        downSizeFilter.setLeafSize(0.1f, 0.1f, 0.1f);
//        downSizeFilter.filter(*transformedPC);
        *laserCloudMap += *transformedPC;

        sensor_msgs::PointCloud2 pc2;
        pcl::toROSMsg(*laserCloudMap, pc2);
        pc2.header.stamp = ros::Time().now();
        pc2.header.frame_id = "/world";
        pubMapAfterNewInput.publish(pc2);

        //saveMap();

        /// Take point cloud in laserCloudArray and downsample it
        //downsampleLaserCloudArray(laserCloudValidNum);

        //createLaserCloudSurround(laserCloudSurroundNum); // maybe not needed because lasercloudsurround should be equal to map

        setTransformationMatrix(transformAftMapped[0],transformAftMapped[1],transformAftMapped[2],transformAftMapped[3],transformAftMapped[4],transformAftMapped[5]);

    }

}

void laserMapping::downsampleInputCloud()
{
    pcl::PointCloud<pcl::PointXYZHSV>::Ptr laserCloudCorner(new pcl::PointCloud<pcl::PointXYZHSV>());
    pcl::PointCloud<pcl::PointXYZHSV>::Ptr laserCloudSurf(new pcl::PointCloud<pcl::PointXYZHSV>());
    pcl::PointCloud<pcl::PointXYZHSV>::Ptr laserCloudCorner2(new pcl::PointCloud<pcl::PointXYZHSV>());
    pcl::PointCloud<pcl::PointXYZHSV>::Ptr laserCloudSurf2(new pcl::PointCloud<pcl::PointXYZHSV>());

    // get rid of this by transmitting corners and surfs separate this is the same as above
    // extract corner and surf features from input cloud
    extractFeatures(laserCloudCorner, laserCloudSurf, laserCloudLast);

    downSampleCloud(laserCloudCorner,laserCloudCorner2,leafSizeCorner);
    *laserCloudCorner2 = *laserCloudCorner;

    downSampleCloud(laserCloudSurf,laserCloudSurf2,leafSizeSurf);
    *laserCloudSurf2 = *laserCloudSurf;

    laserCloudLast->clear();
    *laserCloudLast = *laserCloudCorner2 + *laserCloudSurf2;
}

void laserMapping::downsampleLaserCloudArray(int laserCloudValidNum)
{
    for (int i = 0; i < laserCloudValidNum; i++)
    {
        pcl::PointCloud<pcl::PointXYZHSV>::Ptr laserCloudCubePointer = laserCloudArray[laserCloudValidInd[i]];
        pcl::PointCloud<pcl::PointXYZHSV>::Ptr laserCloudCorner(new pcl::PointCloud<pcl::PointXYZHSV>());
        pcl::PointCloud<pcl::PointXYZHSV>::Ptr laserCloudSurf(new pcl::PointCloud<pcl::PointXYZHSV>());
        pcl::PointCloud<pcl::PointXYZHSV>::Ptr laserCloudCorner2(new pcl::PointCloud<pcl::PointXYZHSV>());
        pcl::PointCloud<pcl::PointXYZHSV>::Ptr laserCloudSurf2(new pcl::PointCloud<pcl::PointXYZHSV>());
        extractFeatures(laserCloudCorner, laserCloudSurf, laserCloudCubePointer);

        //downSampleCloud(laserCloudCorner,laserCloudCorner2,leafSizeCorner);
        *laserCloudCorner2 = *laserCloudCorner;

        //downSampleCloud(laserCloudSurf,laserCloudSurf2,leafSizeSurf);
        *laserCloudSurf2 = *laserCloudSurf;

        laserCloudCubePointer->clear();
        *laserCloudCubePointer = *laserCloudCorner2 + *laserCloudSurf2;
    }
}

void laserMapping::storeMapInCubes(pcl::PointCloud<pcl::PointXYZHSV>::Ptr input)
{
    for (int i = 0; i < input->points.size(); i++)
    {
        if (fabs(input->points[i].x) > 1.2 || fabs(input->points[i].y) > 1.2 || fabs(input->points[i].z) > 1.2)
        {
            // transform point to current estimate
            pcl::PointXYZHSV point;
            pointAssociateToMapEig(&input->points[i], &point);

            int cubeI = int((point.x + 10.0) / 20.0) + laserCloudCenWidth;
            int cubeJ = int((point.y + 10.0) / 20.0) + laserCloudCenHeight;
            int cubeK = int((point.z + 10.0) / 20.0) + laserCloudCenDepth;

            if (point.x + 10.0 < 0) cubeI--;
            if (point.y + 10.0 < 0) cubeJ--;
            if (point.z + 10.0 < 0) cubeK--;

            int cubeInd = cubeI + laserCloudWidth * cubeJ + laserCloudWidth * laserCloudHeight * cubeK;
            //std::cout << "cubeInd=" << cubeInd << std::endl;
            if (cubeInd>0 && cubeInd<laserCloudNum)
                laserCloudArray[cubeInd]->push_back(point);
        }
    }
}

void laserMapping::saveMap()
{
    for (int i = 0; i < laserCloudNum; i++) {
        std::stringstream filename;
        filename << "map_" << i << ".csv";
        writerToFile.writePCLToCSV(filename.str(),laserCloudArray[i]);
    }
}

void laserMapping::setTransformationMatrix(double rx, double ry, double rz, double tx, double ty, double tz)
{
    ROS_WARN ("RESULT tx=%f, ty=%f, tz=%f, rx=%f, ry=%f, rz=%f",tx,ty,tz,rx,ry,rz);
    Eigen::Quaterniond q;
    geometry_msgs::Quaternion geoQuat = tf::createQuaternionMsgFromRollPitchYaw(rx, ry, rz);
    q.x() = geoQuat.x;
    q.y() = geoQuat.y;
    q.z() = geoQuat.z;
    q.w() = geoQuat.w;
    Eigen::Matrix3d R = q.matrix();
    T_transform << R(0,0), R(0,1), R(0,2), tx,
            R(1,0), R(1,1), R(1,2), ty,
            R(2,0), R(2,1), R(2,2), tz,
            0, 0, 0, 1;
}

void laserMapping::processSurfPoints(pcl::PointCloud<pcl::PointXYZHSV>::Ptr laserCloudSurfFromMap, int iterCount, pcl::PointXYZHSV pointSel, float deltaT)
{
    std::vector<float> pointSearchSqDis;
    std::vector<int> pointSearchInd;
    kdtreeSurfFromMap->nearestKSearch(pointSel, 5, pointSearchInd, pointSearchSqDis);




    if (pointSearchSqDis[4] < 1.0) {
        for (int j = 0; j < 5; j++) {
            matA0.at<float>(j, 0) = laserCloudSurfFromMap->points[pointSearchInd[j]].x;
            matA0.at<float>(j, 1) = laserCloudSurfFromMap->points[pointSearchInd[j]].y;
            matA0.at<float>(j, 2) = laserCloudSurfFromMap->points[pointSearchInd[j]].z;
        }
        // A0=[points] B0 = -1
        cv::solve(matA0, matB0, matX0, cv::DECOMP_QR);

        float pa = matX0.at<float>(0, 0);
        float pb = matX0.at<float>(1, 0);
        float pc = matX0.at<float>(2, 0);
        float pd = 1;

        float ps = sqrt(pa * pa + pb * pb + pc * pc);
        pa /= ps;
        pb /= ps;
        pc /= ps;
        pd /= ps;

        bool planeValid = true;
        for (int j = 0; j < 5; j++) {
            if (fabs(pa * laserCloudSurfFromMap->points[pointSearchInd[j]].x +
                     pb * laserCloudSurfFromMap->points[pointSearchInd[j]].y +
                     pc * laserCloudSurfFromMap->points[pointSearchInd[j]].z + pd) > 0.05) {
                planeValid = false;
                break;
            }
        }

        if (planeValid) {
            float pd2 = pa * pointSel.x + pb * pointSel.y + pc * pointSel.z + pd;

            pointProj = pointSel;
            pointProj.x -= pa * pd2;
            pointProj.y -= pb * pd2;
            pointProj.z -= pc * pd2;

            float s = 1;
            if (iterCount >= 6) {
                s = 1 - 8 * fabs(pd2) / sqrt(sqrt(pointSel.x * pointSel.x  + pointSel.y * pointSel.y + pointSel.z * pointSel.z));
            }
            //s = 1;
            coeff.x = s * pa;
            coeff.y = s * pb;
            coeff.z = s * pc;
            coeff.h = s * pd2;

            if (s > 0.2) {
                laserCloudOri->push_back(pointOri);
                //laserCloudSel->push_back(pointSel);
                //laserCloudProj->push_back(pointProj);
                laserCloudMapCorres->push_back(laserCloudSurfFromMap->points[pointSearchInd[0]]);
                //laserCloudCorr->push_back(laserCloudSurfFromMap->points[pointSearchInd[1]]);
                //laserCloudCorr->push_back(laserCloudSurfFromMap->points[pointSearchInd[2]]);
                //laserCloudCorr->push_back(laserCloudSurfFromMap->points[pointSearchInd[3]]);
                //laserCloudCorr->push_back(laserCloudSurfFromMap->points[pointSearchInd[4]]);
                coeffSel->push_back(coeff);
            }
        }
    }
}



// my version
// searchPoint is the transformed version of pointOriginal by current estimate
void laserMapping::processCorner(pcl::PointCloud<pcl::PointXYZHSV>::Ptr laserCloudCornerFromMap, pcl::PointXYZHSV &searchPoint, pcl::PointXYZHSV &pointOriginal)
{
    int K = 5;
    std::vector<int> pointIdxNKNSearch(K);
    std::vector<float> pointNKNSquaredDistance(K);

    if (kdtreeCornerFromMap->nearestKSearch(searchPoint, K, pointIdxNKNSearch, pointNKNSquaredDistance) > 0 )
    {
        if (pointNKNSquaredDistance[4] < 1.0)
        {
            float cx = 0;
            float cy = 0;
            float cz = 0;
            for (int j = 0; j < 5; j++)
            {
                cx += laserCloudCornerFromMap->points[pointIdxNKNSearch[j]].x;
                cy += laserCloudCornerFromMap->points[pointIdxNKNSearch[j]].y;
                cz += laserCloudCornerFromMap->points[pointIdxNKNSearch[j]].z;
            }
            cx /= 5;
            cy /= 5;
            cz /= 5;

            float a11 = 0;
            float a12 = 0;
            float a13 = 0;
            float a22 = 0;
            float a23 = 0;
            float a33 = 0;
            for (int j = 0; j < 5; j++)
            {
                float ax = laserCloudCornerFromMap->points[pointIdxNKNSearch[j]].x - cx;
                float ay = laserCloudCornerFromMap->points[pointIdxNKNSearch[j]].y - cy;
                float az = laserCloudCornerFromMap->points[pointIdxNKNSearch[j]].z - cz;

                a11 += ax * ax;
                a12 += ax * ay;
                a13 += ax * az;
                a22 += ay * ay;
                a23 += ay * az;
                a33 += az * az;
            }

            Eigen::Matrix3f A1;
            A1 << a11, a12, a13, a12, a22, a23, a13, a23, a33;
            A1 = A1 / 5;

            Eigen::EigenSolver<Eigen::Matrix3f> es;
            es.compute(A1,true);
            Eigen::Matrix<std::complex<float>, 3, 1>  eig = es.eigenvalues();
            Eigen::Matrix<std::complex<float>, 3, 3> eig_vec = es.eigenvectors();




            // One eigenvalue should be significantly larger
            if (eig(0, 0).real() > 3 * eig(0, 1).real())
            {
                // Current point
                Eigen::Vector3f X_k_1(searchPoint.x, searchPoint.y, searchPoint.z);

                // Corresponding point 1
                float x1 = cx + 0.1 * eig_vec(0, 0).real();
                float y1 = cy + 0.1 * eig_vec(0, 1).real();
                float z1 = cz + 0.1 * eig_vec(0, 2).real();
                Eigen::Vector3f X_k_j(x1, y1, z1);


                // Corresponding point 2
                float x2 = cx - 0.1 * eig_vec(0, 0).real();
                float y2 = cy - 0.1 * eig_vec(0, 1).real();
                float z2 = cz - 0.1 * eig_vec(0, 2).real();
                Eigen::Vector3f X_k_l(x2, y2, z2);

                // Compute point to line distance
                Eigen::Vector3f left = X_k_1-X_k_j;
                Eigen::Vector3f crossp = left.cross(X_k_1-X_k_l);
                float a012 = crossp.norm();
                Eigen::Vector3f diff = X_k_j - X_k_l;
                float l12 = diff.norm();
                float ld2 = a012 / l12; // point to line distance



                float la = ((y1 - y2)*((X_k_1(0) - x1)*(X_k_1(1) - y2) - (X_k_1(0) - x2)*(X_k_1(1) - y1))
                            + (z1 - z2)*((X_k_1(0) - x1)*(X_k_1(2) - z2) - (X_k_1(0) - x2)*(X_k_1(2) - z1))) / a012 / l12;

                float lb = -((x1 - x2)*((X_k_1(0) - x1)*(X_k_1(1) - y2) - (X_k_1(0) - x2)*(X_k_1(1) - y1))
                             - (z1 - z2)*((X_k_1(1) - y1)*(X_k_1(2) - z2) - (X_k_1(1) - y2)*(X_k_1(2) - z1))) / a012 / l12;

                float lc = -((x1 - x2)*((X_k_1(0) - x1)*(X_k_1(2) - z2) - (X_k_1(0) - x2)*(X_k_1(2) - z1))
                             + (y1 - y2)*((X_k_1(1) - y1)*(X_k_1(2) - z2) - (X_k_1(1) - y2)*(X_k_1(2) - z1))) / a012 / l12;


                /// makes no sense because it is not used
                pointProj = searchPoint;
                pointProj.x -= la * ld2;
                pointProj.y -= lb * ld2;
                pointProj.z -= lc * ld2;

                float s = 2 * (1 - 8 * fabs(ld2));

                coeff.x = s * la;
                coeff.y = s * lb;
                coeff.z = s * lc;
                coeff.h = s * ld2;

                ///
                observing_field.edgePoint = X_k_1;
                observing_field.linePoint_1 = X_k_j;
                observing_field.linePoint_2 = X_k_l;
                observing_field.point_line_distance = ld2;

                if (s > 0.4)
                {
                    laserCloudOri->push_back(pointOriginal);
                    coeffSel->push_back(coeff);
                }
            }
        }
    }
}

void laserMapping::doICP(pcl::PointCloud<pcl::PointXYZHSV>::Ptr laserCloudSurfFromMap, pcl::PointCloud<pcl::PointXYZHSV>::Ptr laserCloudCornerFromMap)
{
    if (laserCloudSurfFromMap->points.size() < 500)
        return;

    kdtreeCornerFromMap->setInputCloud(laserCloudCornerFromMap);
    kdtreeSurfFromMap->setInputCloud(laserCloudSurfFromMap);

    ///

    float deltaR, deltaT;
    for (int iterCount = 0; iterCount < maxIteration; iterCount++)
    {
        laserCloudOri->clear();
        //laserCloudSel->clear();
        laserCloudMapCorres->clear();
        //laserCloudProj->clear();
        coeffSel->clear();

        /// make association
        associate(laserCloudSurfFromMap,iterCount,deltaT);

        solveEigen(deltaR,deltaT);

        std::cout << "[Mapping] iter: " << iterCount << ", deltaR: " << deltaR << ", deltaT: " << deltaT << std::endl;

        if (deltaR < icp_R_break && deltaT < icp_T_break) {
            std::cout << "[MAPPING] deltaR:" << deltaR << " < " << icp_R_break << " && deltaT: " << deltaT << " < " << icp_T_break << std::endl;
            break;
        }
    }
    if (deltaR > icp_R_break || deltaT > icp_T_break) {
        std::cout << "[MAPPING] Out of bound deltaR: " << deltaR << " , deltaT: " << deltaT << std::endl;
    }
}

void laserMapping::doICP_new(pcl::PointCloud<pcl::PointXYZHSV>::Ptr laserCloudSurfFromMap, pcl::PointCloud<pcl::PointXYZHSV>::Ptr laserCloudCornerFromMap)
{
    if (laserCloudSurfFromMap->points.size() < 500)
        return;

    icp.transformation[3] = transformTobeMapped[0];
    icp.transformation[4] = transformTobeMapped[1];
    icp.transformation[5] = transformTobeMapped[2];
    icp.transformation[0] = transformTobeMapped[3];
    icp.transformation[1] = transformTobeMapped[4];
    icp.transformation[2] = transformTobeMapped[5];

    icp.setInputCloud(laserCloudLast);
    icp.setSurfaceMap(laserCloudSurfFromMap);
    icp.doICP();


    transformTobeMapped[0] = icp.transformation[3];
    transformTobeMapped[1] = icp.transformation[4];
    transformTobeMapped[2] = icp.transformation[5];
    transformTobeMapped[3] = icp.transformation[0];
    transformTobeMapped[4] = icp.transformation[1];
    transformTobeMapped[5] = icp.transformation[2];


    icp.reset();

}

void laserMapping::associate(pcl::PointCloud<pcl::PointXYZHSV>::Ptr laserCloudSurfFromMap, int iterCount, float deltaT)
{
    iter_data temp;
    temp.iterCount = iterCount;

    for (int i = 0; i < laserCloudLast->points.size(); i++)
    {
        if (fabs(laserCloudLast->points[i].x > 1.2) || fabs(laserCloudLast->points[i].y > 1.2) || fabs(laserCloudLast->points[i].z > 1.2))
        {
            //pcl::PointXYZHSV pointOriginal = laserCloudLast->points[i];
            pcl::PointXYZHSV pointSel;
            pointOri = laserCloudLast->points[i];
            if (false)
            {
                if (sqrt(pointOri.x * pointOri.x  + pointOri.y * pointOri.y + pointOri.z * pointOri.z) > 16.0)
                {
                    pointAssociateToMapEig(&pointOri, &pointSel); // predict input by with current estimate
                    if (fabs(pointOri.v) < 0.05 || fabs(pointOri.v + 1) < 0.05)
                    {
                        processSurfPoints(laserCloudSurfFromMap, iterCount, pointSel,deltaT);
                    } else
                    {
                        //processCorner(pointSel,pointOriginal);
                        //processCorner();
                        temp.edge_data_vec.push_back(observing_field);
                    }
                }
            }
            else
            {
                pointAssociateToMapEig(&pointOri, &pointSel); // predict input by with current estimate
                if (fabs(pointOri.v) < 0.05 || fabs(pointOri.v + 1) < 0.05)
                {
                    processSurfPoints(laserCloudSurfFromMap, iterCount, pointSel,deltaT);
                } else
                {
                    //processCorner(pointSel,pointOriginal);
                    //processCorner();
                    temp.edge_data_vec.push_back(observing_field);
                }
            }


        }
    }
    observingData.iter_data_vec.push_back(temp);
}

void laserMapping::solveCV()
{
    int laserCloudSelNum = laserCloudOri->points.size();
    if (laserCloudSelNum < 50) {
        return;
    }
    cv::Mat matA(laserCloudSelNum, 6, CV_32F, cv::Scalar::all(0));
    cv::Mat matAt(6, laserCloudSelNum, CV_32F, cv::Scalar::all(0));
    cv::Mat matAtA(6, 6, CV_32F, cv::Scalar::all(0));
    cv::Mat matB(laserCloudSelNum, 1, CV_32F, cv::Scalar::all(0));
    cv::Mat matAtB(6, 1, CV_32F, cv::Scalar::all(0));
    cv::Mat matX(6, 1, CV_32F, cv::Scalar::all(0));

    float srx = sin(transformTobeMapped[0]);
    float crx = cos(transformTobeMapped[0]);
    float sry = sin(transformTobeMapped[1]);
    float cry = cos(transformTobeMapped[1]);
    float srz = sin(transformTobeMapped[2]);
    float crz = cos(transformTobeMapped[2]);

    for (int i = 0; i < laserCloudSelNum; i++)
    {
        pointOri = laserCloudOri->points[i];
        coeff = coeffSel->points[i];

        // [(crx*sry*srz crx*crz*sry -srx*sry)
        // (-srx*srz -crz*srx -crx)
        // (crx*cry*srz crx*cry*crz -cry*srx) ] * pointOri * coeff
        float arx = (crx*sry*srz*pointOri.x + crx*crz*sry*pointOri.y - srx*sry*pointOri.z) * coeff.x
                + (-srx*srz*pointOri.x - crz*srx*pointOri.y - crx*pointOri.z) * coeff.y
                + (crx*cry*srz*pointOri.x + crx*cry*crz*pointOri.y - cry*srx*pointOri.z) * coeff.z;

        //   [(cry*srx*srz - crz*sry) (sry*srz + cry*crz*srx) crx*cry] * pointOri * coeff.x
        // + [(-cry*crz - srx*sry*srz) (cry*srz - crz*srx*sry) -crx*sry] * pointOri * coeff.z
        //   [(c2*s1*s3 - c3*s2) (s2*s3 + c2*c3*s3) c1*c2] * pointOri * coeff.x
        // + [(-cry*crz - srx*sry*srz) (cry*srz - crz*srx*sry) -crx*sry] * pointOri * coeff.z
        float ary = ((cry*srx*srz - crz*sry)*pointOri.x + (sry*srz + cry*crz*srx)*pointOri.y + crx*cry*pointOri.z) * coeff.x
                + ((-cry*crz - srx*sry*srz)*pointOri.x + (cry*srz - crz*srx*sry)*pointOri.y - crx*sry*pointOri.z) * coeff.z;
        // [((crz*srx*sry - cry*srz)*pointOri.x + (-cry*crz - srx*sry*srz)*pointOri.y),
        // (crx*crz*pointOri.x - crx*srz*pointOri.y),
        // ((sry*srz + cry*crz*srx)*pointOri.x + (crz*sry - cry*srx*srz)*pointOri.y)] * coeff
        float arz = ((crz*srx*sry - cry*srz)*pointOri.x + (-cry*crz - srx*sry*srz)*pointOri.y)*coeff.x
                + (crx*crz*pointOri.x - crx*srz*pointOri.y) * coeff.y
                + ((sry*srz + cry*crz*srx)*pointOri.x + (crz*sry - cry*srx*srz)*pointOri.y)*coeff.z;

        matA.at<float>(i, 0) = arx;
        matA.at<float>(i, 1) = ary;
        matA.at<float>(i, 2) = arz;
        matA.at<float>(i, 3) = coeff.x;
        matA.at<float>(i, 4) = coeff.y;
        matA.at<float>(i, 5) = coeff.z;
        matB.at<float>(i, 0) = -coeff.h;
    }
    cv::transpose(matA, matAt);
    matAtA = matAt * matA;
    matAtB = matAt * matB;
    cv::solve(matAtA, matAtB, matX, cv::DECOMP_QR);

    if (fabs(matX.at<float>(0, 0)) < 0.5 &&
            fabs(matX.at<float>(1, 0)) < 0.5 &&
            fabs(matX.at<float>(2, 0)) < 0.5 &&
            fabs(matX.at<float>(3, 0)) < 1 &&
            fabs(matX.at<float>(4, 0)) < 1 &&
            fabs(matX.at<float>(5, 0)) < 1) {

        transformTobeMapped[0] += matX.at<float>(0, 0);
        transformTobeMapped[1] += matX.at<float>(1, 0);
        transformTobeMapped[2] += matX.at<float>(2, 0);
        transformTobeMapped[3] += matX.at<float>(3, 0);
        transformTobeMapped[4] += matX.at<float>(4, 0);
        transformTobeMapped[5] += matX.at<float>(5, 0);
    } else {
        ROS_WARN ("Odometry update out of bound: tx=%f, ty=%f, tz=%f",matX.at<float>(3, 0),matX.at<float>(4, 0),matX.at<float>(5, 0));
    }

    float deltaR = sqrt(matX.at<float>(0, 0) * 180 / PI * matX.at<float>(0, 0) * 180 / PI
                        + matX.at<float>(1, 0) * 180 / PI * matX.at<float>(1, 0) * 180 / PI
                        + matX.at<float>(2, 0) * 180 / PI * matX.at<float>(2, 0) * 180 / PI);
    float deltaT = sqrt(matX.at<float>(3, 0) * 100 * matX.at<float>(3, 0) * 100
                        + matX.at<float>(4, 0) * 100 * matX.at<float>(4, 0) * 100
                        + matX.at<float>(5, 0) * 100 * matX.at<float>(5, 0) * 100);

    //                if (deltaR < 0.1 && deltaT < 0.1) {
    //                    ROS_WARN ("[MAPPING] deltaR=%f < 0.1 && deltaT=%f < 0.1", deltaR, deltaT);
    //                    break;
    //                }

    //ROS_INFO ("iter: %d, deltaR: %f, deltaT: %f", iterCount, deltaR, deltaT);
}

void laserMapping::solveEigen(float &deltaR, float &deltaT)
{
    int laserCloudSelNum = laserCloudOri->points.size();
    if (laserCloudSelNum < 50) {
        return;
    }
    //cv::Mat matA(laserCloudSelNum, 6, CV_32F, cv::Scalar::all(0));
    Eigen::MatrixXf A(laserCloudSelNum,6);
    //cv::Mat matAt(6, laserCloudSelNum, CV_32F, cv::Scalar::all(0));
    Eigen::MatrixXf At(6,laserCloudSelNum);
    //cv::Mat matAtA(6, 6, CV_32F, cv::Scalar::all(0));
    Eigen::MatrixXf AtA(6,6);
    //cv::Mat matB(laserCloudSelNum, 1, CV_32F, cv::Scalar::all(0));
    Eigen::MatrixXf B(laserCloudSelNum,1);
    //cv::Mat matAtB(6, 1, CV_32F, cv::Scalar::all(0));
    Eigen::MatrixXf AtB(6,1);
    //cv::Mat matX(6, 1, CV_32F, cv::Scalar::all(0));
    Eigen::MatrixXf X(6,1);

    float srx = sin(transformTobeMapped[0]);
    float crx = cos(transformTobeMapped[0]);
    float sry = sin(transformTobeMapped[1]);
    float cry = cos(transformTobeMapped[1]);
    float srz = sin(transformTobeMapped[2]);
    float crz = cos(transformTobeMapped[2]);

    /// paralaizable
    for (int i = 0; i < laserCloudSelNum; i++)
    {
        pointOri = laserCloudOri->points[i];
        coeff = coeffSel->points[i];
        //
        float arx = (crx*sry*srz*pointOri.x + crx*crz*sry*pointOri.y - srx*sry*pointOri.z) * coeff.x
                + (-srx*srz*pointOri.x - crz*srx*pointOri.y - crx*pointOri.z) * coeff.y
                + (crx*cry*srz*pointOri.x + crx*cry*crz*pointOri.y - cry*srx*pointOri.z) * coeff.z;

        float ary = ((cry*srx*srz - crz*sry)*pointOri.x
                     + (sry*srz + cry*crz*srx)*pointOri.y + crx*cry*pointOri.z) * coeff.x
                + ((-cry*crz - srx*sry*srz)*pointOri.x
                   + (cry*srz - crz*srx*sry)*pointOri.y - crx*sry*pointOri.z) * coeff.z;

        float arz = ((crz*srx*sry - cry*srz)*pointOri.x + (-cry*crz - srx*sry*srz)*pointOri.y)*coeff.x
                + (crx*crz*pointOri.x - crx*srz*pointOri.y) * coeff.y
                + ((sry*srz + cry*crz*srx)*pointOri.x + (crz*sry - cry*srx*srz)*pointOri.y)*coeff.z;

        //matA.at<float>(i, 0) = arx;
        A(i,0) = arx;
        //matA.at<float>(i, 1) = ary;
        A(i,1) = ary;
        //matA.at<float>(i, 2) = arz;
        A(i,2) = arz;
        //matA.at<float>(i, 3) = coeff.x;
        A(i,3) = coeff.x;
        //matA.at<float>(i, 4) = coeff.y;
        A(i,4) = coeff.y;
        //matA.at<float>(i, 5) = coeff.z;
        A(i,5) = coeff.z;
        //matB.at<float>(i, 0) = -coeff.h;
        B(i,0) = -coeff.h;
    }
    //cv::transpose(matA, matAt);
    At = A.transpose();
    //matAtA = matAt * matA;
    AtA = At * A;
    //matAtB = matAt * matB;
    AtB = At*B;
    //cv::solve(matAtA, matAtB, matX, cv::DECOMP_QR);

    //X = AtA.colPivHouseholderQr().solve(AtB);

    X = AtA.householderQr().solve(AtB);
    //    if (fabs(matX.at<float>(0, 0)) < 0.5 &&
    //            fabs(matX.at<float>(1, 0)) < 0.5 &&
    //            fabs(matX.at<float>(2, 0)) < 0.5 &&
    //            fabs(matX.at<float>(3, 0)) < 1 &&
    //            fabs(matX.at<float>(4, 0)) < 1 &&
    //            fabs(matX.at<float>(5, 0)) < 1) {
    if (fabs(X(0, 0)) < 0.5 &&
            fabs(X(1, 0)) < 0.5 &&
            fabs(X(2, 0)) < 0.5 &&
            fabs(X(3, 0)) < 1 &&
            fabs(X(4, 0)) < 1 &&
            fabs(X(5, 0)) < 1) {

        transformTobeMapped[0] += X(0, 0);
        transformTobeMapped[1] += X(1, 0);
        transformTobeMapped[2] += X(2, 0);
        transformTobeMapped[3] += X(3, 0);
        transformTobeMapped[4] += X(4, 0);
        transformTobeMapped[5] += X(5, 0);
    } else {
        ROS_WARN ("Odometry update out of bound: tx=%f, ty=%f, tz=%f",X(3, 0),X(4, 0),X(5, 0));
    }

    deltaR = sqrt(X(0, 0) * 180 / PI * X(0, 0) * 180 / PI
                  + X(1, 0) * 180 / PI * X(1, 0) * 180 / PI
                  + X(2, 0) * 180 / PI * X(2, 0) * 180 / PI);
    deltaT = sqrt(X(3, 0) * 100 * X(3, 0) * 100
                  + X(4, 0) * 100 * X(4, 0) * 100
                  + X(5, 0) * 100 * X(5, 0) * 100);

}

// Extracts the corner and surface features from input cloud and stores them in corners and surfs
void laserMapping::extractFeatures(pcl::PointCloud<pcl::PointXYZHSV>::Ptr corners, pcl::PointCloud<pcl::PointXYZHSV>::Ptr surfs, pcl::PointCloud<pcl::PointXYZHSV>::Ptr input)
{
    //corners->clear();
    //surfs->clear();
    for (int i = 0; i < input->points.size(); i++)
    {
        if (fabs(input->points[i].v - 2) < 0.005 || fabs(input->points[i].v - 1) < 0.005)
        {
            corners->push_back(input->points[i]);
        } else
        {
            surfs->push_back(input->points[i]);
        }
    }
}

void laserMapping::createLaserCloudSurround(int laserCloudSurroundNum)
{
    laserCloudSurround->clear();
    for (int i = 0; i < laserCloudSurroundNum; i++)
    {
        pcl::PointCloud<pcl::PointXYZHSV>::Ptr laserCloudCubePointer = laserCloudArray[laserCloudSurroundInd[i]];
        for (int j = 0; j < laserCloudCubePointer->points.size(); j++)
        {   pcl::PointXYZI pointSurround;
            pointSurround.x = laserCloudCubePointer->points[j].x;
            pointSurround.y = laserCloudCubePointer->points[j].y;
            pointSurround.z = laserCloudCubePointer->points[j].z;
            pointSurround.intensity = laserCloudCubePointer->points[j].h;
            laserCloudSurround->push_back(pointSurround);
        }
    }
}

void laserMapping::downSampleCloud(pcl::PointCloud<pcl::PointXYZHSV>::Ptr inPC, pcl::PointCloud<pcl::PointXYZHSV>::Ptr outPC, float leafSize)
{
    outPC->clear(); // make sure target cloud is empty
    pcl::VoxelGrid<pcl::PointXYZHSV> downSizeFilter;
    downSizeFilter.setInputCloud(inPC);
    downSizeFilter.setLeafSize(leafSize, leafSize, leafSize);
    downSizeFilter.filter(*outPC);
}

void laserMapping::processCorner(pcl::PointCloud<pcl::PointXYZHSV>::Ptr laserCloudCornerFromMap, pcl::PointXYZHSV pointSel)
{
    std::vector<float> pointSearchSqDis;
    std::vector<int> pointSearchInd;
    kdtreeCornerFromMap->nearestKSearch(pointSel, 5, pointSearchInd, pointSearchSqDis);

    if (pointSearchSqDis[4] < 1.0)
    {
        float cx = 0;
        float cy = 0;
        float cz = 0;
        for (int j = 0; j < 5; j++) {
            cx += laserCloudCornerFromMap->points[pointSearchInd[j]].x;
            cy += laserCloudCornerFromMap->points[pointSearchInd[j]].y;
            cz += laserCloudCornerFromMap->points[pointSearchInd[j]].z;
        }
        cx /= 5;
        cy /= 5;
        cz /= 5;

        float a11 = 0;
        float a12 = 0;
        float a13 = 0;
        float a22 = 0;
        float a23 = 0;
        float a33 = 0;
        for (int j = 0; j < 5; j++) {
            float ax = laserCloudCornerFromMap->points[pointSearchInd[j]].x - cx;
            float ay = laserCloudCornerFromMap->points[pointSearchInd[j]].y - cy;
            float az = laserCloudCornerFromMap->points[pointSearchInd[j]].z - cz;

            a11 += ax * ax;
            a12 += ax * ay;
            a13 += ax * az;
            a22 += ay * ay;
            a23 += ay * az;
            a33 += az * az;
        }
        a11 /= 5;
        a12 /= 5;
        a13 /= 5;
        a22 /= 5;
        a23 /= 5;
        a33 /= 5;

        matA1.at<float>(0, 0) = a11;
        matA1.at<float>(0, 1) = a12;
        matA1.at<float>(0, 2) = a13;
        matA1.at<float>(1, 0) = a12;
        matA1.at<float>(1, 1) = a22;
        matA1.at<float>(1, 2) = a23;
        matA1.at<float>(2, 0) = a13;
        matA1.at<float>(2, 1) = a23;
        matA1.at<float>(2, 2) = a33;

        cv::eigen(matA1, matD1, matV1);

        if (matD1.at<float>(0, 0) > 3 * matD1.at<float>(0, 1))
        {

            float x0 = pointSel.x;
            float y0 = pointSel.y;
            float z0 = pointSel.z;
            float x1 = cx + 0.1 * matV1.at<float>(0, 0);
            float y1 = cy + 0.1 * matV1.at<float>(0, 1);
            float z1 = cz + 0.1 * matV1.at<float>(0, 2);
            float x2 = cx - 0.1 * matV1.at<float>(0, 0);
            float y2 = cy - 0.1 * matV1.at<float>(0, 1);
            float z2 = cz - 0.1 * matV1.at<float>(0, 2);

            float a012 = sqrt(((x0 - x1)*(y0 - y2) - (x0 - x2)*(y0 - y1))
                              * ((x0 - x1)*(y0 - y2) - (x0 - x2)*(y0 - y1))
                              + ((x0 - x1)*(z0 - z2) - (x0 - x2)*(z0 - z1))
                              * ((x0 - x1)*(z0 - z2) - (x0 - x2)*(z0 - z1))
                              + ((y0 - y1)*(z0 - z2) - (y0 - y2)*(z0 - z1))
                              * ((y0 - y1)*(z0 - z2) - (y0 - y2)*(z0 - z1)));

            float l12 = sqrt((x1 - x2)*(x1 - x2) + (y1 - y2)*(y1 - y2) + (z1 - z2)*(z1 - z2));

            float la = ((y1 - y2)*((x0 - x1)*(y0 - y2) - (x0 - x2)*(y0 - y1))
                        + (z1 - z2)*((x0 - x1)*(z0 - z2) - (x0 - x2)*(z0 - z1))) / a012 / l12;

            float lb = -((x1 - x2)*((x0 - x1)*(y0 - y2) - (x0 - x2)*(y0 - y1))
                         - (z1 - z2)*((y0 - y1)*(z0 - z2) - (y0 - y2)*(z0 - z1))) / a012 / l12;

            float lc = -((x1 - x2)*((x0 - x1)*(z0 - z2) - (x0 - x2)*(z0 - z1))
                         + (y1 - y2)*((y0 - y1)*(z0 - z2) - (y0 - y2)*(z0 - z1))) / a012 / l12;

            float ld2 = a012 / l12;

            pointProj = pointSel;
            pointProj.x -= la * ld2;
            pointProj.y -= lb * ld2;
            pointProj.z -= lc * ld2;

            float s = 2 * (1 - 8 * fabs(ld2));


            coeff.x = s * la;
            coeff.y = s * lb;
            coeff.z = s * lc;
            coeff.h = s * ld2;

            if (s > 0.4)
            {
                laserCloudOri->push_back(pointOri);
                //laserCloudSel->push_back(pointSel);
                //laserCloudProj->push_back(pointProj);
                //laserCloudCorr->push_back(laserCloudCornerFromMap->points[pointSearchInd[0]]);
                //laserCloudCorr->push_back(laserCloudCornerFromMap->points[pointSearchInd[1]]);
                //laserCloudCorr->push_back(laserCloudCornerFromMap->points[pointSearchInd[2]]);
                //laserCloudCorr->push_back(laserCloudCornerFromMap->points[pointSearchInd[3]]);
                //laserCloudCorr->push_back(laserCloudCornerFromMap->points[pointSearchInd[4]]);
                coeffSel->push_back(coeff);
            }

        }
    }
}

}
