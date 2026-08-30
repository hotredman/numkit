// toolboxes/image/tests/image_e2e_io_test.cpp
//
// End-to-end interpreter integration tests for imread, imwrite, and imfinfo
// across PNG, JPEG, BMP, TGA, PNM, and TIFF.

#include <gtest/gtest.h>
#include <numkit/core/engine.hpp>
#include <numkit/fs/vfs.hpp>
#include <numkit/value/value.hpp>

#include <filesystem>
#include <random>

using namespace numkit;

class ImageE2EIoTest : public ::testing::Test {
public:
    StandardEngine engine;

    void SetUp() override {
            }

    void TearDown() override {
        std::error_code ec;
        for (const auto &f : {"test_gray.png", "test_rgb.png", "photo.jpg", "matrix.bmp", "image.tga", "image.pgm"}) {
            std::filesystem::remove(f, ec);
        }
    }

    Value eval(const std::string &code) {
        return engine.eval(code);
    }
};

TEST_F(ImageE2EIoTest, PngGrayscaleAndRgbWorkflow) {
    eval(R"(
        gray = uint8([10 20 30; 40 50 60]);
        imwrite(gray, 'test_gray.png');
        read_gray = imread('test_gray.png');
        info_gray = imfinfo('test_gray.png');
    )");

    Value readGray = eval("read_gray;");
    EXPECT_EQ(readGray.dims().rows(), 2u);
    EXPECT_EQ(readGray.dims().cols(), 3u);
    EXPECT_EQ(readGray.type(), ValueType::UINT8);

    Value infoGray = eval("info_gray;");
    EXPECT_EQ(infoGray.field("Format").toString(), "png");
    EXPECT_EQ(infoGray.field("Width").toScalar(), 3.0);
    EXPECT_EQ(infoGray.field("Height").toScalar(), 2.0);
    EXPECT_EQ(infoGray.field("NumberOfChannels").toScalar(), 1.0);
    EXPECT_EQ(infoGray.field("NumberOfSamples").toScalar(), 1.0);
    EXPECT_EQ(infoGray.field("ColorType").toString(), "grayscale");

    // RGB 3D
    eval(R"(
        rgb = uint8(cat(3, [255 0; 0 255], [0 255; 0 0], [0 0; 255 0]));
        imwrite(rgb, 'test_rgb.png');
        read_rgb = imread('test_rgb.png');
        info_rgb = imfinfo('test_rgb.png');
    )");

    Value readRgb = eval("read_rgb;");
    EXPECT_EQ(readRgb.dims().rows(), 2u);
    EXPECT_EQ(readRgb.dims().cols(), 2u);
    EXPECT_EQ(readRgb.dims().pages(), 3u);

    Value infoRgb = eval("info_rgb;");
    EXPECT_EQ(infoRgb.field("NumberOfChannels").toScalar(), 3.0);
    EXPECT_EQ(infoRgb.field("NumberOfSamples").toScalar(), 3.0);
    EXPECT_EQ(infoRgb.field("ColorType").toString(), "truecolor");
}

TEST_F(ImageE2EIoTest, JpegWorkflow) {
    eval(R"(
        rgb = uint8(cat(3, [100 120; 140 160], [50 70; 90 110], [20 40; 60 80]));
        imwrite(rgb, 'photo.jpg');
        read_jpg = imread('photo.jpg');
        info_jpg = imfinfo('photo.jpg');
    )");

    Value readJpg = eval("read_jpg;");
    EXPECT_EQ(readJpg.dims().rows(), 2u);
    EXPECT_EQ(readJpg.dims().cols(), 2u);
    EXPECT_EQ(readJpg.dims().pages(), 3u);

    Value infoJpg = eval("info_jpg;");
    EXPECT_EQ(infoJpg.field("Format").toString(), "jpg");
    EXPECT_EQ(infoJpg.field("Width").toScalar(), 2.0);
    EXPECT_EQ(infoJpg.field("Height").toScalar(), 2.0);
}

TEST_F(ImageE2EIoTest, BmpWorkflow) {
    eval(R"(
        A = uint8([1 2 3 4 5; 6 7 8 9 10; 11 12 13 14 15]);
        imwrite(A, 'matrix.bmp');
        read_bmp = imread('matrix.bmp');
        info_bmp = imfinfo('matrix.bmp');
    )");

    Value readBmp = eval("read_bmp;");
    EXPECT_EQ(readBmp.dims().rows(), 3u);
    EXPECT_EQ(readBmp.dims().cols(), 5u);

    Value infoBmp = eval("info_bmp;");
    EXPECT_EQ(infoBmp.field("Format").toString(), "bmp");
    EXPECT_EQ(infoBmp.field("Width").toScalar(), 5.0);
    EXPECT_EQ(infoBmp.field("Height").toScalar(), 3.0);
}

TEST_F(ImageE2EIoTest, TgaWorkflow) {
    eval(R"(
        A = uint8([10 20; 30 40; 50 60]);
        imwrite(A, 'image.tga');
        read_tga = imread('image.tga');
        info_tga = imfinfo('image.tga');
    )");

    Value readTga = eval("read_tga;");
    EXPECT_EQ(readTga.dims().rows(), 3u);
    EXPECT_EQ(readTga.dims().cols(), 2u);

    Value infoTga = eval("info_tga;");
    EXPECT_EQ(infoTga.field("Format").toString(), "tga");
    EXPECT_EQ(infoTga.field("Width").toScalar(), 2.0);
    EXPECT_EQ(infoTga.field("Height").toScalar(), 3.0);
}

TEST_F(ImageE2EIoTest, PnmPpmPgmWorkflow) {
    eval(R"(
        pgm = uint8([100 200; 50 150]);
        imwrite(pgm, 'image.pgm');
        read_pgm = imread('image.pgm');
        info_pgm = imfinfo('image.pgm');
    )");

    Value readPgm = eval("read_pgm;");
    EXPECT_EQ(readPgm.dims().rows(), 2u);
    EXPECT_EQ(readPgm.dims().cols(), 2u);

    Value infoPgm = eval("info_pgm;");
    EXPECT_EQ(infoPgm.field("Format").toString(), "pnm");
    EXPECT_EQ(infoPgm.field("Width").toScalar(), 2.0);
    EXPECT_EQ(infoPgm.field("Height").toScalar(), 2.0);
}
