/*
 * Copyright (c) 2017-2018 ARM Limited.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONCLCTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "arm_compute/core/Types.h"
#include "arm_compute/core/utils/misc/ShapeCalculator.h"
#include "arm_compute/runtime/NEON/functions/NEDepthwiseConvolutionLayer.h"
#include "arm_compute/runtime/Tensor.h"
#include "arm_compute/runtime/TensorAllocator.h"
#include "tests/NEON/Accessor.h"
#include "tests/PaddingCalculator.h"
#include "tests/datasets/DepthwiseConvolutionLayerDataset.h"
#include "tests/framework/Asserts.h"
#include "tests/framework/Macros.h"
#include "tests/framework/datasets/Datasets.h"
#include "tests/validation/Validation.h"
#include "tests/validation/fixtures/DepthwiseConvolutionLayerFixture.h"

namespace arm_compute
{
namespace test
{
namespace validation
{
using namespace arm_compute::misc::shape_calculator;

namespace
{
constexpr RelativeTolerance<float>   tolerance_f32(0.01f); /**< Tolerance value for comparing reference's output against implementation's output for DataType::F32 */
constexpr AbsoluteTolerance<uint8_t> tolerance_qasymm8(1); /**< Tolerance value for comparing reference's output against implementation's output for DataType::QASYMM8 */

const auto depth_multipliers = framework::dataset::make("DepthMultiplier", { 1, 2, 3 });
} // namespace

TEST_SUITE(NEON)
TEST_SUITE(DepthwiseConvLayer)

DATA_TEST_CASE(Configuration, framework::DatasetMode::ALL, combine(combine(framework::dataset::concat(datasets::SmallDepthwiseConvolutionLayerDataset3x3(),
                                                                                                      datasets::LargeDepthwiseConvolutionLayerDataset3x3()),
                                                                           depth_multipliers),
                                                                   framework::dataset::make("DataType", DataType::F32)),
               input_shape, kernel_size, info, depth_multiplier, data_type)
{
    // Get shapes
    TensorShape weights_shape(kernel_size.width, kernel_size.height);

    const TensorInfo  in_info(input_shape, 1, data_type);
    const TensorInfo  we_info(weights_shape, 1, data_type);
    const TensorShape output_shape = compute_depthwise_convolution_shape(in_info, we_info, info, depth_multiplier);

    weights_shape.set(2, output_shape.z());

    // Create tensors
    Tensor            src     = create_tensor<Tensor>(input_shape, data_type);
    Tensor            dst     = create_tensor<Tensor>(output_shape, data_type);
    Tensor            weights = create_tensor<Tensor>(weights_shape, data_type);
    const TensorShape bias_shape(weights_shape[2]);
    Tensor            bias = create_tensor<Tensor>(bias_shape, data_type);

    ARM_COMPUTE_EXPECT(src.info()->is_resizable(), framework::LogLevel::ERRORS);
    ARM_COMPUTE_EXPECT(dst.info()->is_resizable(), framework::LogLevel::ERRORS);
    ARM_COMPUTE_EXPECT(weights.info()->is_resizable(), framework::LogLevel::ERRORS);
    ARM_COMPUTE_EXPECT(bias.info()->is_resizable(), framework::LogLevel::ERRORS);

    // Create and configure function
    NEDepthwiseConvolutionLayer3x3 depthwise_layer;
    depthwise_layer.configure(&src, &weights, &bias, &dst, info, depth_multiplier);

    // Validate valid region
    const ValidRegion input_valid_region   = shape_to_valid_region(input_shape);
    const ValidRegion output_valid_region  = shape_to_valid_region(output_shape);
    const ValidRegion weights_valid_region = shape_to_valid_region(weights_shape);
    const ValidRegion bias_valid_region    = shape_to_valid_region(bias_shape);

    validate(src.info()->valid_region(), input_valid_region);
    validate(dst.info()->valid_region(), output_valid_region);
    validate(weights.info()->valid_region(), weights_valid_region);
    validate(bias.info()->valid_region(), bias_valid_region);

    // Validate padding
    bool              is_optimized_run = NEDepthwiseConvolutionLayer3x3Kernel::is_optimized_execution_possible(input_shape, info, data_type, depth_multiplier, DataLayout::NCHW);
    const int         step_non_opt_dwc = 16 >> info.stride().first;
    const int         step_bias_add    = 16 / src.info()->element_size();
    const int         step             = is_optimized_run ? step_bias_add : std::max(step_non_opt_dwc, step_bias_add);
    const PaddingSize padding          = PaddingCalculator(output_shape.x(), step).required_padding();
    validate(dst.info()->padding(), padding);
}

TEST_SUITE(Float)
TEST_SUITE(F32)
TEST_SUITE(Generic)
template <typename T>
using NEDepthwiseConvolutionLayerFixture = DepthwiseConvolutionLayerValidationFixture<Tensor, Accessor, NEDepthwiseConvolutionLayer, T>;
FIXTURE_DATA_TEST_CASE(RunSmall, NEDepthwiseConvolutionLayerFixture<float>, framework::DatasetMode::PRECOMMIT, combine(combine(combine(datasets::SmallDepthwiseConvolutionLayerDataset(),
                                                                                                                       depth_multipliers),
                                                                                                                       framework::dataset::make("DataType",
                                                                                                                               DataType::F32)),
                                                                                                                       framework::dataset::make("DataLayout", DataLayout::NCHW)))
{
    validate(Accessor(_target), _reference, tolerance_f32);
}
FIXTURE_DATA_TEST_CASE(RunLarge, NEDepthwiseConvolutionLayerFixture<float>, framework::DatasetMode::NIGHTLY, combine(combine(combine(datasets::LargeDepthwiseConvolutionLayerDataset(),
                                                                                                                     depth_multipliers),
                                                                                                                     framework::dataset::make("DataType",
                                                                                                                             DataType::F32)),
                                                                                                                     framework::dataset::make("DataLayout", DataLayout::NCHW)))
{
    validate(Accessor(_target), _reference, tolerance_f32);
}
TEST_SUITE_END()

TEST_SUITE(W3x3)
template <typename T>
using NEDepthwiseConvolutionLayerFixture3x3 = DepthwiseConvolutionLayerValidationFixture<Tensor, Accessor, NEDepthwiseConvolutionLayer3x3, T>;
FIXTURE_DATA_TEST_CASE(RunSmall, NEDepthwiseConvolutionLayerFixture3x3<float>, framework::DatasetMode::ALL, combine(combine(combine(datasets::SmallDepthwiseConvolutionLayerDataset3x3(),
                                                                                                                    depth_multipliers),
                                                                                                                    framework::dataset::make("DataType",
                                                                                                                            DataType::F32)),
                                                                                                                    framework::dataset::make("DataLayout", DataLayout::NCHW)))
{
    validate(Accessor(_target), _reference, tolerance_f32);
}
FIXTURE_DATA_TEST_CASE(RunLarge, NEDepthwiseConvolutionLayerFixture3x3<float>, framework::DatasetMode::NIGHTLY, combine(combine(combine(datasets::LargeDepthwiseConvolutionLayerDataset3x3(),
                                                                                                                        depth_multipliers),
                                                                                                                        framework::dataset::make("DataType",
                                                                                                                                DataType::F32)),
                                                                                                                        framework::dataset::make("DataLayout", DataLayout::NCHW)))
{
    validate(Accessor(_target), _reference, tolerance_f32);
}
FIXTURE_DATA_TEST_CASE(RunOptimized, NEDepthwiseConvolutionLayerFixture3x3<float>, framework::DatasetMode::ALL, combine(combine(combine(datasets::OptimizedDepthwiseConvolutionLayerDataset3x3(),
                                                                                                                        framework::dataset::make("DepthMultiplier", 1)),
                                                                                                                        framework::dataset::make("DataType",
                                                                                                                                DataType::F32)),
                                                                                                                        framework::dataset::make("DataLayout", { DataLayout::NCHW, DataLayout::NHWC })))
{
    validate(Accessor(_target), _reference, tolerance_f32);
}
TEST_SUITE_END()
TEST_SUITE_END()

TEST_SUITE_END()

template <typename T>
using NEDepthwiseConvolutionLayerQuantizedFixture3x3 = DepthwiseConvolutionLayerValidationQuantizedFixture<Tensor, Accessor, NEDepthwiseConvolutionLayer3x3, T>;
template <typename T>
using NEDepthwiseConvolutionLayerQuantizedFixture = DepthwiseConvolutionLayerValidationQuantizedFixture<Tensor, Accessor, NEDepthwiseConvolutionLayer, T>;

TEST_SUITE(Quantized)
TEST_SUITE(QASYMM8)
TEST_SUITE(Generic)
FIXTURE_DATA_TEST_CASE(RunSmall, NEDepthwiseConvolutionLayerQuantizedFixture<uint8_t>, framework::DatasetMode::PRECOMMIT,
                       combine(combine(combine(combine(datasets::SmallDepthwiseConvolutionLayerDataset(),
                                                       depth_multipliers),
                                               framework::dataset::make("DataType", DataType::QASYMM8)),
                                       framework::dataset::make("QuantizationInfo", { QuantizationInfo(0.5f, 10) })),
                               framework::dataset::make("DataLayout", DataLayout::NCHW)))
{
    validate(Accessor(_target), _reference, tolerance_qasymm8);
}
TEST_SUITE_END()
TEST_SUITE(W3x3)
FIXTURE_DATA_TEST_CASE(RunSmall, NEDepthwiseConvolutionLayerQuantizedFixture3x3<uint8_t>, framework::DatasetMode::PRECOMMIT,
                       combine(combine(combine(combine(datasets::SmallDepthwiseConvolutionLayerDataset3x3(), depth_multipliers),
                                               framework::dataset::make("DataType", DataType::QASYMM8)),
                                       framework::dataset::make("QuantizationInfo", { QuantizationInfo(0.5f, 10) })),
                               framework::dataset::make("DataLayout", DataLayout::NCHW)))
{
    validate(Accessor(_target), _reference, tolerance_qasymm8);
}
FIXTURE_DATA_TEST_CASE(RunLarge, NEDepthwiseConvolutionLayerQuantizedFixture3x3<uint8_t>, framework::DatasetMode::NIGHTLY,
                       combine(combine(combine(combine(datasets::LargeDepthwiseConvolutionLayerDataset3x3(),
                                                       depth_multipliers),
                                               framework::dataset::make("DataType", DataType::QASYMM8)),
                                       framework::dataset::make("QuantizationInfo", { QuantizationInfo(0.5f, 10) })),
                               framework::dataset::make("DataLayout", DataLayout::NCHW)))
{
    validate(Accessor(_target), _reference, tolerance_qasymm8);
}
TEST_SUITE_END()
TEST_SUITE_END()
TEST_SUITE_END()

TEST_SUITE_END()
TEST_SUITE_END()
} // namespace validation
} // namespace test
} // namespace arm_compute
