
/**
 * @file main.cpp
 * @author guoqing (1337841346@qq.com)
 * @brief KinectFusionApp 运行的主文件
 * @version 0.1
 * @date 2019-07-01
 * 
 * @copyright Copyright (c) 2019
 * 
 */

// KinectFusionApp 自身的依赖
#include <kinectfusion.h>
#include <depth_camera.h>
#include <util.h>

// C++ 标准库
#include <iostream>
#include <fstream>
#include <iomanip>


// 为了整洁，忽略在导入 opencv 的时候产生的警告
#pragma GCC diagnostic push                 // 忽略警告的代码段 -- 开始
#pragma GCC diagnostic ignored "-Wall"      // 设置忽略的警告的类型
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Weffc++"
#include <opencv2/highgui.hpp>
#pragma GCC diagnostic pop                  // 忽略警告的代码段 -- 结束

// C++ 命令行参数解析工具
#include <cxxopts.hpp>
// C++ 解析 toml 格式配置文件的工具
#include <cpptoml.h>

// ? 目测是深度数据的存储路径
std::string data_path {};
// ? 目测是 .bag 数据集中的topic
std::string recording_name {};

// TODO
auto make_configuration(const std::shared_ptr<cpptoml::table>& toml_config)
{
    kinectfusion::GlobalConfiguration configuration;

    // cpptoml only supports int64_t, so we need to explicitly cast to int to suppress the warning
    auto volume_size_values = *toml_config->get_qualified_array_of<int64_t>("kinectfusion.volume_size");
    configuration.volume_size = make_int3(static_cast<int>(volume_size_values[0]),
                                          static_cast<int>(volume_size_values[1]),
                                          static_cast<int>(volume_size_values[2]));
    configuration.voxel_scale = static_cast<float>(*toml_config->get_qualified_as<double>("kinectfusion.voxel_scale"));
    configuration.bfilter_kernel_size = *toml_config->get_qualified_as<int>("kinectfusion.bfilter_kernel_size");
    configuration.bfilter_color_sigma  = static_cast<float>(*toml_config->get_qualified_as<double>("kinectfusion.bfilter_color_sigma"));
    configuration.bfilter_spatial_sigma  = static_cast<float>(*toml_config->get_qualified_as<double>("kinectfusion.bfilter_spatial_sigma"));
    configuration.init_depth  = static_cast<float>(*toml_config->get_qualified_as<double>("kinectfusion.init_depth"));
    configuration.use_output_frame = *toml_config->get_qualified_as<bool>("kinectfusion.use_output_frame");
    configuration.truncation_distance  = static_cast<float>(*toml_config->get_qualified_as<double>("kinectfusion.truncation_distance"));
    configuration.depth_cutoff_distance  = static_cast<float>(*toml_config->get_qualified_as<double>("kinectfusion.depth_cutoff_distance"));
    configuration.num_levels  = *toml_config->get_qualified_as<int>("kinectfusion.num_levels");
    configuration.triangles_buffer_size  = *toml_config->get_qualified_as<int>("kinectfusion.triangles_buffer_size");
    configuration.pointcloud_buffer_size  = *toml_config->get_qualified_as<int>("kinectfusion.pointcloud_buffer_size");
    configuration.distance_threshold  = static_cast<float>(*toml_config->get_qualified_as<double>("kinectfusion.distance_threshold"));
    configuration.angle_threshold  = static_cast<float>(*toml_config->get_qualified_as<double>("kinectfusion.angle_threshold"));
    auto icp_iterations_values = *toml_config->get_qualified_array_of<int64_t>("kinectfusion.icp_iterations");
    configuration.icp_iterations = {icp_iterations_values.begin(), icp_iterations_values.end()};

    return configuration;
}

// TODO
auto make_camera(const std::shared_ptr<cpptoml::table>& toml_config)
{
    std::unique_ptr<DepthCamera> camera;

    const auto camera_type = *toml_config->get_qualified_as<std::string>("camera.type");
    if (camera_type == "Pseudo") {
        std::stringstream source_path {};
        source_path << data_path << "source/" << recording_name << "/";
        camera = std::make_unique<PseudoCamera>(source_path.str());
    } else if (camera_type == "Xtion") {
        camera = std::make_unique<XtionCamera>();
    } else if (camera_type == "RealSense") {
        if(*toml_config->get_qualified_as<bool>("camera.realsense.live")) {
            camera = std::make_unique<RealSenseCamera>();
        } else {
            std::stringstream source_file {};
            source_file << data_path << "source/" << recording_name << ".bag";
            camera = std::make_unique<RealSenseCamera>(source_file.str());
        }
    } else {
        throw std::logic_error("There is no implementation for the camera type you specified.");
    }

    
    return camera;
}

// TODO
void main_loop(const std::unique_ptr<DepthCamera> camera, const kinectfusion::GlobalConfiguration& configuration)
{
    kinectfusion::Pipeline pipeline { camera->get_parameters(), configuration };

    cv::namedWindow("Pipeline Output");
    for (bool end = false; !end;) {
        //1 Get frame
        InputFrame frame = camera->grab_frame();

        //2 Process frame
        bool success = pipeline.process_frame(frame.depth_map, frame.color_map);
        if (!success)
            std::cout << "Frame could not be processed" << std::endl;

        //3 Display the output
        cv::imshow("Pipeline Output", pipeline.get_last_model_frame());

        switch (cv::waitKey(1)) {
            case 'a': { // Save all available data
                std::cout << "Saving all ..." << std::endl;
                std::cout << "Saving poses ..." << std::endl;
                auto poses = pipeline.get_poses();

                for (size_t i = 0; i < poses.size(); ++i) {
                    std::stringstream file_name {};
                    file_name << data_path << "poses/" << recording_name << "/seq_pose" << std::setfill('0')
                              << std::setw(5) << i << ".txt";
                    std::ofstream { file_name.str() } << poses[i] << std::endl;
                }

                std::cout << "Extracting mesh ..." << std::endl;
                auto mesh = pipeline.extract_mesh();
                std::cout << "Saving mesh ..." << std::endl;
                std::stringstream file_name {};
                file_name << data_path << "meshes/" << recording_name << ".ply";
                kinectfusion::export_ply(file_name.str(), mesh);
                end = true;
                break;
            }
            case 'p': { // Save poses only
                std::cout << "Saving poses ..." << std::endl;
                auto poses = pipeline.get_poses();

                for (size_t i = 0; i < poses.size(); ++i) {
                    std::stringstream file_name {};
                    file_name << data_path << "poses/" << recording_name << "/seq_pose" << std::setfill('0')
                              << std::setw(5) << i << ".txt";
                    std::ofstream { file_name.str() } << poses[i] << std::endl;
                }
                end = true;
                break;
            }
            case 'm': { // Save mesh only
                std::cout << "Extracting mesh ..." << std::endl;
                auto mesh = pipeline.extract_mesh();
                std::cout << "Saving mesh ..." << std::endl;
                std::stringstream file_name {};
                file_name << data_path << "meshes/" << recording_name << ".ply";
                kinectfusion::export_ply(file_name.str(), mesh);
                end = true;
                break;
            }
            case ' ': // Save nothing
                end = true;
                break;
            default:
                break;
        }
    }
}

// TODO
void setup_cuda_device()
{
    auto n_devices = cv::cuda::getCudaEnabledDeviceCount();
    std::cout << "Found " << n_devices << " CUDA devices" << std::endl;
    for (int device_idx = 0; device_idx < n_devices; ++device_idx) {
        cv::cuda::DeviceInfo info { device_idx };
        std::cout << "Device #" << device_idx << ": " << info.name()
                  << " with " << info.totalMemory() / 1048576 << "MB total memory" << std::endl;
    }

    // Hardcoded to first device; change if necessary
    std::cout << "Using device #0" << std::endl;
    cv::cuda::setDevice(0);
}

/**
 * @brief 程序入口
 * @param[in] argc argc
 * @param[in] argv argv
 * @return int     运行状态
 */
int main(int argc, char* argv[])
{
    // TODO 这些解析的部分我现在还都看不懂，得先拿这两个库做个操练才行

    // Parse command line options
    // step 1 解析命令行参数
    cxxopts::Options options { "KinectFusionApp",
                               "Sample application for KinectFusionLib, a modern implementation of the KinectFusion approach"};
    options.add_options()("c,config", "Configuration filename", cxxopts::value<std::string>());
    auto program_arguments = options.parse(argc, argv);
    if (program_arguments.count("config") == 0)
        throw std::invalid_argument("You have to specify a path to the configuration file");

    // Parse TOML configuration file
    // step 2 解析 TOML 配置文件
    auto toml_config = cpptoml::parse_file(program_arguments["config"].as<std::string>());
    data_path = *toml_config->get_as<std::string>("data_path");
    recording_name = *toml_config->get_as<std::string>("recording_name");

    // Print info about available CUDA devices and specify device to use
    // step 3 设置CUDA设备
    setup_cuda_device();

    // Start the program's main loop
    // step 4 进入主循环
    main_loop(
            make_camera(toml_config),
            make_configuration(toml_config)
    );

    return EXIT_SUCCESS;
}
