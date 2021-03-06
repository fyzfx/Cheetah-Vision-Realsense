// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2018 Intel Corporation. All Rights Reserved.

#pragma once

#include <vector>
#include <string>
#include "l500-device.h"
#include "stream.h"
#include "l500-depth.h"

namespace librealsense
{
    class l500_color : public virtual l500_device
    {
    public:
        std::shared_ptr<uvc_sensor> create_color_device(std::shared_ptr<context> ctx,
            const std::vector<platform::uvc_device_info>& color_devices_info);

        l500_color(std::shared_ptr<context> ctx,
            const platform::backend_device_group& group);

        std::vector<tagged_profile> get_profiles_tags() const override;

    protected:
        std::shared_ptr<stream_interface> _color_stream;

    private:
        friend class l500_color_sensor;

        uint8_t _color_device_idx = -1;

        lazy<std::vector<uint8_t>> _color_intrinsics_table_raw;
        lazy<std::vector<uint8_t>> _color_extrinsics_table_raw;
        std::shared_ptr<lazy<rs2_extrinsics>> _color_extrinsic;

        std::vector<uint8_t> get_raw_intrinsics_table() const;
        std::vector<uint8_t> get_raw_extrinsics_table() const;
    };

    class l500_color_sensor : public uvc_sensor, public video_sensor_interface
        {
        public:
            explicit l500_color_sensor(l500_color* owner, std::shared_ptr<platform::uvc_device> uvc_device,
                std::unique_ptr<frame_timestamp_reader> timestamp_reader,
                std::shared_ptr<context> ctx)
                : uvc_sensor("RGB Camera", uvc_device, move(timestamp_reader), owner), _owner(owner)
            {}

            rs2_intrinsics get_intrinsics(const stream_profile& profile) const override
            {
                using namespace ivcam2;

                auto intrinsic = check_calib<intrinsic_rgb>(*_owner->_color_intrinsics_table_raw);

                auto num_of_res = intrinsic->resolution.num_of_resolutions;

                for (auto i = 0; i < num_of_res; i++)
                {
                    auto model = intrinsic->resolution.intrinsic_resolution[i];
                    if (model.height == profile.height && model.width == profile.width)
                    {
                        rs2_intrinsics intrinsics;
                        intrinsics.width = model.width;
                        intrinsics.height = model.height;
                        intrinsics.fx = model.ipm.focal_length.x;
                        intrinsics.fy = model.ipm.focal_length.y;
                        intrinsics.ppx = model.ipm.principal_point.x;
                        intrinsics.ppy = model.ipm.principal_point.y;
                        return intrinsics;
                    }
                }
                throw std::runtime_error(to_string() << "intrinsics for resolution "<< profile.width <<","<< profile.height<< " doesn't exist");
            }

            stream_profiles init_stream_profiles() override
            {
                auto lock = environment::get_instance().get_extrinsics_graph().lock();

                auto results = uvc_sensor::init_stream_profiles();

                for (auto p : results)
                {
                    // Register stream types
                    if (p->get_stream_type() == RS2_STREAM_COLOR)
                    {
                        assign_stream(_owner->_color_stream, p);
                    }

                    // Register intrinsics
                    auto video = dynamic_cast<video_stream_profile_interface*>(p.get());

                    auto profile = to_profile(p.get());
                    std::weak_ptr<l500_color_sensor> wp =
                        std::dynamic_pointer_cast<l500_color_sensor>(this->shared_from_this());
                    video->set_intrinsics([profile, wp]()
                    {
                        auto sp = wp.lock();
                        if (sp)
                            return sp->get_intrinsics(profile);
                        else
                            return rs2_intrinsics{};
                    });
                }

                return results;
            }

            processing_blocks get_recommended_processing_blocks() const override
            {
                return get_color_recommended_proccesing_blocks();
            }

        private:
            const l500_color* _owner;
        };

}
