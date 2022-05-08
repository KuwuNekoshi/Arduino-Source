/*  Map Detector
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#include <QImage>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/Time.h"
#include "CommonFramework/ImageTools/BinaryImage_FilterRgb32.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
// #include "CommonFramework/ImageTools/ImageFilter.h"
#include "CommonFramework/ImageTools/SolidColorTest.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "Kernels/Waterfill/Kernels_Waterfill_Session.h"
#include "Kernels/Algorithm/Kernels_Algorithm_DisjointSet.h"
#include "PokemonLA_BerryTreeDetector.h"
#include "PokemonLA/PokemonLA_Locations.h"

#include <iostream>
#include <algorithm>
#include <sstream>
#include <map>
using std::cout;
using std::endl;

namespace PokemonAutomation{

using Kernels::Waterfill::WaterfillObject;
using Kernels::DisjointSet;

namespace NintendoSwitch{
namespace PokemonLA{


void set_color_to_image(const WaterfillObject& obj, const QColor& color, QImage& image, size_t offset_x, size_t offset_y){
    for(int x = 0; x < obj.width(); x++){
        for(int y = 0; y < obj.height(); y++){
            if (obj.object->get(obj.min_x + x, obj.min_y + y)){
                image.setPixelColor(offset_x + obj.min_x + x, offset_y + obj.min_y + y, color);
                // cout << "Set color at " << offset_x + object.min_x + obj.min_x + x << ", " << offset_y + object.min_y + obj.min_y + y << endl;
            }
        }
    }
}

void set_color_to_image(const PackedBinaryMatrix2& matrix, const QColor& color, QImage& image, size_t offset_x, size_t offset_y){
    for(int x = 0; x < matrix.width(); x++){
        for(int y = 0; y < matrix.height(); y++){
            if (matrix.get(x, y)){
                image.setPixelColor(offset_x + x, offset_y + y, color);
            }
        }
    }
}

// Merge boxes that overlap with each other, meaning ImagePixelBox::overlap_with() return true.
// return a vector of boxes that are merged versions of the input boxes.
// Note this function will order the input vector.
// Note the returned boxes may still overlap with each other. This is because the algorithm
// essentially find the connected components of the input boxes and return the bounding box of each
// connected component. Although any two connected components don't overlap with each other, their
// bounding boxes may still overlap.
std::vector<ImagePixelBox> merge_overlapping_boxes(std::vector<ImagePixelBox>& boxes){
    // sort boxes according to min_x. If two boxes have same min_x, compare min_y
    std::sort(boxes.begin(), boxes.end(), [](const ImagePixelBox& a, const ImagePixelBox& b){
        return a.min_x < b.min_x ? true : (a.min_x == b.min_x ? a.min_y < b.min_y : false);
    });

    DisjointSet disjoint_set(boxes.size());

    for (size_t i = 0; i < boxes.size(); i++){
        for(size_t j = i + 1; j < boxes.size(); j++){
            // box_j and the ones behind it won't overlap with cur_box.
            if (boxes[j].min_x >= boxes[i].max_x){
                // break;
            }

            if (boxes[i].overlap(boxes[j])){
                disjoint_set.merge(i, j);
            }
        }
    }

    std::map<size_t, ImagePixelBox> merged_boxes;
    for(size_t i = 0; i < boxes.size(); i++){
        size_t rep_box = disjoint_set.find(i);
        auto it = merged_boxes.find(rep_box);
        if (it == merged_boxes.end()){
            it = merged_boxes.emplace(rep_box, boxes[rep_box]).first;
        }
        
        if(rep_box != i){
            // merge the child box i with the rep_box
            it->second.merge_with(boxes[i]);
        }
    }

    std::vector<ImagePixelBox> output_boxes;
    for(const auto& p : merged_boxes){
        output_boxes.push_back(p.second);
    }

    return output_boxes;
}


bool detect_sphere(const Kernels::Waterfill::WaterfillObject& object, QImage* image, size_t offset_x, size_t offset_y){
    PackedBinaryMatrix2 matrix = object.packed_matrix();
    size_t width = matrix.width();
    size_t height = matrix.height();

    //  Sort all pixels by distance from center.
    size_t center_x = object.center_x() - object.min_x;
    size_t center_y = object.center_y() - object.min_y;
    std::map<uint64_t, size_t> distances;
    for (size_t r = 0; r < height; r++){
        for (size_t c = 0; c < width; c++){
            if (matrix.get(c, r)){
                size_t dist_x = c - center_x;
                size_t dist_y = r - center_y;
                uint64_t distance_sqr = (uint64_t)dist_x*dist_x + (uint64_t)dist_y*dist_y;
                distances[distance_sqr]++;
            }
        }
    }

    cout << "Object area in detection sphere: " << object.area << endl;
    double threshold = 0.3;
    if (object.area >= 900){
        threshold = 0.5;
    } else if (object.area >= 100) {
        // For area size (assuming square shape) from 10.0 to 30.0, linearly raise
        // threshold from 0.3 to 0.5.
        threshold = 0.3 + 0.2 * (std::sqrt(object.area) - 10.0) / 20.0;
    }

    //  Filter out the bottom 30%-50%.
    size_t stop = (size_t)(threshold * object.area);
    size_t count = 0;
    uint64_t distance = 0;
    for (auto& item : distances){
        count += item.second;
        if (count >= stop){
            distance = item.first;
            break;
        }
    }

    for (size_t r = 0; r < height; r++){
        for (size_t c = 0; c < width; c++){
            size_t dist_x = c - center_x;
            size_t dist_y = r - center_y;
            uint64_t distance_sqr = (uint64_t)dist_x*dist_x + (uint64_t)dist_y*dist_y;
            if (distance_sqr < distance){
                matrix.set(c, r, false);
            }
        }
    }

    PackedBinaryMatrix2 matrix2 = matrix.copy();
    auto session = Kernels::Waterfill::make_WaterfillSession(matrix2);
    auto finder = session->make_iterator(1);
    Kernels::Waterfill::WaterfillObject obj;
    size_t num_regions = 0;
    const bool keep_object = (image != nullptr);
    while (finder->find_next(obj, keep_object)){
        num_regions++;

        if (image){
            QColor color = QColor(0, 0, 255);
            if (num_regions == 1){
                color = QColor(128, 128, 255);
            }
            // cout << "cut area " << obj.area << endl;
            for(int x = 0; x < obj.width(); x++){
                for(int y = 0; y < obj.height(); y++){
                    if (obj.object->get(obj.min_x + x, obj.min_y + y)){
                        image->setPixelColor(object.min_x + offset_x + obj.min_x + x, offset_y + object.min_y + obj.min_y + y, color);
                        // cout << "Set color at " << offset_x + object.min_x + obj.min_x + x << ", " << offset_y + object.min_y + obj.min_y + y << endl;
                    }
                }
            }
        }
    }

    std::cout << "num regions " << num_regions << std::endl;

    return num_regions == 1;
}

namespace{

using ColorPair = std::pair<Color, Color>;

// Color of a tree at a certain time/weather
struct TreeColor{
    ColorPair fruit_core;

    ColorPair fruit_full;

    ColorPair leave;

    TreeColor(ColorPair fruit_core, ColorPair fruit_full, ColorPair leave) : fruit_core(fruit_core), fruit_full(fruit_full), leave(leave) {}
};

enum class BerryTreeType{
    APRICON,
    ORAN,
    LEPPA, // fieldlands: blue-leaf leppa
    SITRUS  // fieldlands: blue-leaf sitrus
};

std::map<MapRegion, std::map<BerryTreeType, std::vector<TreeColor>>> all_tree_colors = {
    {
        MapRegion::FIELDLANDS,
        {
            {
                BerryTreeType::APRICON,
                {
                    // Nightfall color
                    TreeColor(
                        ColorPair(Color(110, 65, 35), Color(160, 110, 65)),
                        ColorPair(Color(60, 45, 10), Color(210, 160, 130)),
                        ColorPair(Color(0, 20, 0), Color(60, 85, 40))
                    )
                }
            }
        }
    }
};




std::string to_str(const ImagePixelBox& box){
    std::ostringstream os;
    os << "(" << box.min_x << ", " << box.max_x << ", " << box.min_y << ", " << box.max_y << ")";
    return os.str();
}


}  // anonymous namespace



BerryTreeDetector::BerryTreeDetector()
    : VisualInferenceCallback("BerryTreeDetector")
{}

void BerryTreeDetector::make_overlays(VideoOverlaySet& items) const{
}

//  Return true if the inference session should stop.
bool BerryTreeDetector::process_frame(const QImage& frame, WallClock timestamp){
    
    // First, use the core-color of the berry to find candidate locations:

    // Run waterfill:
    // pixels within the color range are marked as 1 in matrices:
    PackedBinaryMatrix2 matrix = compress_rgb32_to_binary_multirange(frame, {
            // light color:
            {combine_rgb(110, 60, 25), combine_rgb(190, 130, 105)},
            // dark color
            {combine_rgb(90, 40, 10), combine_rgb(130, 60, 50)}
            // {combine_rgb(70, 30, 0), combine_rgb(130, 80, 100)} 
            // {combine_rgb(70, 30, 0), combine_rgb(160, 110, 100)}
        });

    QImage debug_image = frame.copy();

    //  Create the session and reuse it for each matrix.
    std::unique_ptr<Kernels::Waterfill::WaterfillSession> session = Kernels::Waterfill::make_WaterfillSession();
    session->set_source(matrix);

    //  Create object iterator for this matrix.
    const size_t core_color_min_area = frame.width() / 100;
    const size_t core_color_max_area = frame.width() / 2;
    auto finder = session->make_iterator(core_color_min_area);
    Kernels::Waterfill::WaterfillObject object;

    // item area is the lower left of the screen with the current selected item/pokemon.
    // We won't do any berry detection on that area as it's covered with item/pokemon sprites.
    const int item_area_x = int(frame.width() * 0.78);
    const int item_area_y = int(frame.height() * 0.752);
    const ImagePixelBox character_area(int(frame.width() * 0.39), int(frame.height() * 0.77), int(frame.width() * 0.61), frame.height());

    std::vector<ImagePixelBox> berry_candidate_areas;
    //  Iterate the objects for this matrix.
    while (finder->find_next(object, false)){
        //  Do something with candidate. (stored in "object")
        if (object.area > core_color_max_area){
            continue;
        }

        // Skip the item area (lower left of the screen)
        if (object.center_x() >= item_area_x && object.center_y() >= item_area_y){
            continue;
        }
        // Skip if it overlaps with the main character.
        if (character_area.inside(object.center_x(), object.center_y())){
            continue;
        }
        
        // The size of the core-color area
        size_t size = std::max(object.width(), object.height());

        // Increase the size to cover the full berry and surrounding area:
        size_t radius = size;

        ImagePixelBox box;
        box.min_x = object.center_x() - radius;
        box.max_x = object.center_x() + radius;
        box.min_y = object.center_y() - radius;
        box.max_y = object.center_y() + radius;
        box.clip(debug_image.width(), debug_image.height());

        // std::cout << "core berry color " << to_str(box) << std::endl;
        // draw_box(debug_image, box, uint32_t(COLOR_GREEN), 1);
        
        berry_candidate_areas.push_back(box);
    }

    berry_candidate_areas = merge_overlapping_boxes(berry_candidate_areas);

    std::vector<ImagePixelBox> berry_boxes;

    for(const auto& candidate_box: berry_candidate_areas){
        std::cout << "candidate box " << to_str(candidate_box) << std::endl;
        // std::cout << std::endl;

        draw_box(debug_image, candidate_box, uint32_t(COLOR_RED), 1);

        // Waterfill using the full range of berry color:
        auto full_color_matrix = compress_rgb32_to_binary_multirange(
            extract_box_reference(frame, candidate_box), {
            // light color:
            {combine_rgb(140, 100, 30), combine_rgb(255, 200, 180)},
            // bright color
            // {combine_rgb(70, 30, 0), combine_rgb(130, 80, 100)} 
            {combine_rgb(70, 30, 0), combine_rgb(160, 110, 100)}
        });

        // set_color_to_image(full_color_matrix, QColor(0, 0, 255), debug_image, candidate_box.min_x, candidate_box.min_y);

        session->set_source(full_color_matrix);
        const size_t berry_min_area = core_color_min_area * 3;
        finder = session->make_iterator(berry_min_area);

        // Find the object at the center of the berry candidate region
        const bool keep_object = true;
        if (session->find_object_on_bit(object, keep_object, candidate_box.width()/2, candidate_box.height()/2) == false){
            continue;
        }
        if (object.area > core_color_max_area){
            continue;
        }
        if (detect_sphere(object, &debug_image, candidate_box.min_x, candidate_box.min_y) == false){
            continue;
        }

        ImagePixelBox box;
        box.min_x = candidate_box.min_x + object.center_x() - object.width()/2;
        box.max_x = candidate_box.min_x + object.center_x() + object.width()/2;
        box.min_y = candidate_box.min_y + object.center_y() - object.height()/2;
        box.max_y = candidate_box.min_y + object.center_y() + object.height()/2;

        std::cout << "full-color region #pixels: " << object.area << " at " << box.center_x() << " " << box.center_y() << std::endl;

        berry_boxes.push_back(box);
    }

    // Check leaf color around berries
    for(const auto& berry_box : berry_boxes){
        draw_box(debug_image, berry_box, uint32_t(COLOR_GREEN), 1);

        ImagePixelBox enlarged_box;
        size_t enlarged_width = berry_box.width() + berry_box.width() / 2;
        size_t enlarged_height = berry_box.height() + berry_box.height() / 2;
        size_t enlarged_box_size = enlarged_width * enlarged_height;
        enlarged_box.min_x = berry_box.center_x() - enlarged_width/2;
        enlarged_box.max_x = berry_box.center_x() + enlarged_width/2;
        enlarged_box.min_y = berry_box.center_y() - enlarged_height/2;
        enlarged_box.max_y = berry_box.center_y() + enlarged_height/2;

        size_t potential_leaf_area = enlarged_box_size - berry_box.width() * berry_box.height();

        auto leaf_matrix = compress_rgb32_to_binary_multirange(
            extract_box_reference(frame, enlarged_box), {
                // {combine_rgb(0, 0, 0), combine_rgb(60, 85, 50)}, // old values
                {combine_rgb(0, 0, 0), combine_rgb(20, 90, 40)},
                {combine_rgb(0, 50, 0), combine_rgb(20, 90, 60)},
                // avoid dark blue color: 9, 36, 60 and (0, 6, 28)
                // {combine_rgb(20, 30, 0), combine_rgb(20, 90, 50)},
            }
        );

        // for(int x = 0; x < enlarged_box.width(); x++){
        //     for(int y = 0; y < enlarged_box.height(); y++){
        //         if (leaf_matrix.get(x, y)){
        //             debug_image.setPixelColor(enlarged_box.min_x + x, enlarged_box.min_y + y, QColor(255, 0, 0));
        //         }
        //     }
        // }

        session->set_source(leaf_matrix);
        finder = session->make_iterator(0);

        size_t num_leaf_bits = 0;
        while (finder->find_next(object, false)){
            num_leaf_bits += object.area;
        }

        std::cout << "candidate berry at " << berry_box.center_x() << " " << berry_box.center_y() << ", leaf " << num_leaf_bits << "/" << potential_leaf_area << std::endl;
        // draw_box(debug_image, enlarged_box, uint32_t(COLOR_CYAN), 2);
        // draw_box(debug_image, berry_box, uint32_t(COLOR_BLUE), 2);
        if (num_leaf_bits > potential_leaf_area * 2 / 3){
            std::cout << "berry at " << berry_box.center_x() << " " << berry_box.center_y() << ", leaf " << num_leaf_bits << "/" << potential_leaf_area << std::endl;
            draw_box(debug_image, berry_box, uint32_t(COLOR_BLUE), 3);
        }
    }

    

    // for(int x = 0; x < debug_image.width(); x++){
    //     for(int y = 0; y < debug_image.height(); y++){
    //         if (matrices[0].get(x, y)){
    //             debug_image.setPixelColor(x, y, QColor(255, 0, 0));
    //         }
    //     }
    // }
    std::cout << "Saving inference debug_image" << std::endl;
    debug_image.save("./test_berry_tree_output.png");

    return true;
}



}
}
}
