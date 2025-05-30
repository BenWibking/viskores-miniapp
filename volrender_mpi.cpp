// ABOUTME: MPI parallel volume rendering demo using back-to-front gather-to-rank-0 compositing
// ABOUTME: Distributes AMR data across ranks and composites final volume rendering on rank 0

#include <viskores/cont/Initialize.h>
#include <viskores/cont/MergePartitionedDataSet.h>
#include <viskores/cont/PartitionedDataSet.h>
#include <viskores/filter/entity_extraction/Threshold.h>
#include <viskores/rendering/Actor.h>
#include <viskores/rendering/CanvasRayTracer.h>
#include <viskores/rendering/MapperRayTracer.h>
#include <viskores/rendering/MapperVolume.h>
#include <viskores/rendering/Scene.h>
#include <viskores/rendering/View3D.h>
#include <viskores/source/Amr.h>

#include <mpi.h>
#include <iostream>
#include <memory>
#include <cmath>
#include <vector>
#include <algorithm>

#include "data.hpp"

// Custom data generation function that creates per-rank AMR data  
auto getMPIPhysicsCodeProxyData(int rank, int numRanks, viskores::Vec3f_64& minBounds, viskores::Vec3f_64& maxBounds) 
    -> std::tuple<viskores::cont::PartitionedDataSet, std::string> {
  
  // Generate the full AMR dataset on all ranks
  const int nx = 8;
  const int nlevels = 3;
  viskores::source::Amr amrDataGenerator{};
  amrDataGenerator.SetDimension(3); // 3D
  amrDataGenerator.SetCellsPerDimension(nx);
  amrDataGenerator.SetNumberOfLevels(nlevels);
  
  viskores::cont::PartitionedDataSet const fullAmrData = amrDataGenerator.Execute();
  
  // Perform round-robin selection: each rank takes every numRanks-th partition
  viskores::cont::PartitionedDataSet rankData;
  
  int totalPartitions = fullAmrData.GetNumberOfPartitions();
  if (rank == 0) {
    std::cout << "Total AMR partitions: " << totalPartitions << std::endl;
    std::cout << "Distributing partitions round-robin across " << numRanks << " ranks" << std::endl;
  }
  
  // Add partitions using round-robin assignment
  for (int i = rank; i < totalPartitions; i += numRanks) {
    rankData.AppendPartition(fullAmrData.GetPartition(i));
    if (rank == 0 && i < 10) { // Log first few for rank 0
      std::cout << "Rank " << rank << " gets partition " << i << std::endl;
    }
  }
  
  // Calculate spatial bounds for this rank's partitions for visibility ordering
  minBounds = viskores::make_Vec(std::numeric_limits<double>::max(), 
                                 std::numeric_limits<double>::max(), 
                                 std::numeric_limits<double>::max());
  maxBounds = viskores::make_Vec(-std::numeric_limits<double>::max(), 
                                 -std::numeric_limits<double>::max(), 
                                 -std::numeric_limits<double>::max());
  
  // Compute actual bounds of this rank's data
  int numMyPartitions = rankData.GetNumberOfPartitions();
  for (int i = 0; i < numMyPartitions; ++i) {
    auto partition = rankData.GetPartition(i);
    auto bounds = partition.GetCoordinateSystem().GetBounds();
    
    // Update min bounds
    minBounds[0] = std::min(minBounds[0], bounds.X.Min);
    minBounds[1] = std::min(minBounds[1], bounds.Y.Min);
    minBounds[2] = std::min(minBounds[2], bounds.Z.Min);
    
    // Update max bounds  
    maxBounds[0] = std::max(maxBounds[0], bounds.X.Max);
    maxBounds[1] = std::max(maxBounds[1], bounds.Y.Max);
    maxBounds[2] = std::max(maxBounds[2], bounds.Z.Max);
  }
  
  // Print info for all ranks
  std::cout << "Rank " << rank << " has " << numMyPartitions << " partitions" << std::endl;
  std::cout << "Rank " << rank << " bounds: (" << minBounds[0] << "," << minBounds[1] << "," << minBounds[2] 
            << ") to (" << maxBounds[0] << "," << maxBounds[1] << "," << maxBounds[2] << ")" << std::endl;
  
  std::string fieldName = "RTDataCells";
  return std::make_tuple(rankData, fieldName);
}

// Calculate maximum distance from camera to any point in the bounds
double calculateMaxDistanceFromCamera(const viskores::Vec3f_64& cameraPos, 
                                      const viskores::Vec3f_64& minBounds, 
                                      const viskores::Vec3f_64& maxBounds) {
  double maxDist = 0.0;
  
  // Check all 8 corners of the bounding box
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 2; ++j) {
      for (int k = 0; k < 2; ++k) {
        viskores::Vec3f_64 corner = viskores::make_Vec(
          i == 0 ? minBounds[0] : maxBounds[0],
          j == 0 ? minBounds[1] : maxBounds[1], 
          k == 0 ? minBounds[2] : maxBounds[2]
        );
        
        viskores::Vec3f_64 diff = corner - cameraPos;
        double dist = viskores::Magnitude(diff);
        maxDist = std::max(maxDist, dist);
      }
    }
  }
  
  return maxDist;
}

// Simple gather-to-rank-0 compositing with back-to-front ordering
void gatherAndCompositeImages(const viskores::rendering::CanvasRayTracer& localCanvas,
                              const std::vector<std::pair<int, double>>& orderedRanks,
                              int rank, int numRanks, const std::string& filename) {
  
  // Find this rank's position in the visibility ordering
  int visibilityOrder = -1;
  for (size_t i = 0; i < orderedRanks.size(); ++i) {
    if (orderedRanks[i].first == rank) {
      visibilityOrder = static_cast<int>(i);
      break;
    }
  }
  
  // Save local partial image with visibility ordering in filename
  std::string partialFilename = "partial_order_" + std::to_string(visibilityOrder) + 
                                "_rank_" + std::to_string(rank) + ".png";
  localCanvas.SaveAs(partialFilename);
  if (rank == 0) {
    std::cout << "Saved partial images with visibility ordering as partial_order_*_rank_*.png" << std::endl;
  }
  int width = localCanvas.GetWidth();
  int height = localCanvas.GetHeight();
  int totalPixels = width * height;
  
  // Get local color buffer
  auto localColorBuffer = localCanvas.GetColorBuffer();
  auto localColorPortal = localColorBuffer.ReadPortal();
  
  // Prepare data for MPI transfer
  std::vector<float> localColorData(totalPixels * 4); // RGBA
  
  // Copy local data into transfer buffer
  for (int i = 0; i < totalPixels; ++i) {
    viskores::Vec4f_32 color = localColorPortal.Get(i);
    localColorData[i*4 + 0] = color[0]; // R
    localColorData[i*4 + 1] = color[1]; // G
    localColorData[i*4 + 2] = color[2]; // B
    localColorData[i*4 + 3] = color[3]; // A
  }
  
  if (rank == 0) {
    // Rank 0 receives from all ranks and composites in back-to-front order
    std::vector<std::vector<float>> allColorData(numRanks, std::vector<float>(totalPixels * 4));
    
    // Receive data from all ranks (including self)
    for (int r = 0; r < numRanks; ++r) {
      if (r == 0) {
        // Copy own data
        allColorData[0] = localColorData;
      } else {
        MPI_Recv(allColorData[r].data(), totalPixels * 4, MPI_FLOAT, r, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      }
    }
    
    // Save all received partial images with visibility ordering in filenames
    for (int r = 0; r < numRanks; ++r) {
      // Find this rank's position in the visibility ordering
      int rankVisibilityOrder = -1;
      for (size_t i = 0; i < orderedRanks.size(); ++i) {
        if (orderedRanks[i].first == r) {
          rankVisibilityOrder = static_cast<int>(i);
          break;
        }
      }
      
      viskores::rendering::CanvasRayTracer partialCanvas(width, height);
      auto partialColorBuffer = partialCanvas.GetColorBuffer();
      auto partialColorPortal = partialColorBuffer.WritePortal();
      
      // Reconstruct image from received data
      for (int i = 0; i < totalPixels; ++i) {
        viskores::Vec4f_32 color(
          allColorData[r][i*4 + 0],
          allColorData[r][i*4 + 1], 
          allColorData[r][i*4 + 2],
          allColorData[r][i*4 + 3]
        );
        partialColorPortal.Set(i, color);
      }
      
      std::string receivedPartialFilename = "received_order_" + std::to_string(rankVisibilityOrder) + 
                                            "_rank_" + std::to_string(r) + ".png";
      partialCanvas.SaveAs(receivedPartialFilename);
    }
    std::cout << "Saved received partial images with visibility ordering as received_order_*_rank_*.png" << std::endl;
    
    // Create final composited image
    viskores::rendering::CanvasRayTracer finalCanvas(width, height);
    auto finalColorBuffer = finalCanvas.GetColorBuffer();
    auto finalColorPortal = finalColorBuffer.WritePortal();
    
    // Initialize with transparent background for back-to-front compositing
    viskores::Vec4f_32 bgColor(0.0f, 0.0f, 0.0f, 0.0f);
    for (int i = 0; i < totalPixels; ++i) {
      finalColorPortal.Set(i, bgColor);
    }
    
    // Composite in back-to-front order based on distance
    std::cout << "Compositing images in back-to-front order..." << std::endl;
    for (const auto& [rankId, distance] : orderedRanks) {
      std::cout << "  Compositing rank " << rankId << " (distance: " << distance << ")" << std::endl;
      
      for (int pixelIdx = 0; pixelIdx < totalPixels; ++pixelIdx) {
        int colorOffset = pixelIdx * 4;
        
        viskores::Vec4f_32 incomingColor(
          allColorData[rankId][colorOffset + 0],
          allColorData[rankId][colorOffset + 1], 
          allColorData[rankId][colorOffset + 2],
          allColorData[rankId][colorOffset + 3]
        );
        
        // Only composite if there's significant alpha
        if (incomingColor[3] > 0.01f) {
          viskores::Vec4f_32 currentColor = finalColorPortal.Get(pixelIdx);
          
          // Back-to-front alpha blending: incoming layer OVER accumulated result
          float incomingAlpha = incomingColor[3];
          float currentAlpha = currentColor[3];
          float oneMinusCurrentAlpha = 1.0f - currentAlpha;
          viskores::Vec4f_32 blendedColor(
            currentColor[0] + incomingColor[0] * incomingAlpha * oneMinusCurrentAlpha,
            currentColor[1] + incomingColor[1] * incomingAlpha * oneMinusCurrentAlpha,
            currentColor[2] + incomingColor[2] * incomingAlpha * oneMinusCurrentAlpha,
            currentAlpha + incomingAlpha * oneMinusCurrentAlpha
          );
          
          finalColorPortal.Set(pixelIdx, blendedColor);
        }
      }
    }
    
    // Apply final background blending for pixels with alpha < 1.0
    viskores::Vec4f_32 finalBgColor(0.2f, 0.2f, 0.2f, 1.0f);
    for (int i = 0; i < totalPixels; ++i) {
      viskores::Vec4f_32 currentColor = finalColorPortal.Get(i);
      if (currentColor[3] < 1.0f) {
        float alpha = currentColor[3];
        float oneMinusAlpha = 1.0f - alpha;
        viskores::Vec4f_32 finalColor(
          currentColor[0] + finalBgColor[0] * oneMinusAlpha,
          currentColor[1] + finalBgColor[1] * oneMinusAlpha,
          currentColor[2] + finalBgColor[2] * oneMinusAlpha,
          1.0f
        );
        finalColorPortal.Set(i, finalColor);
      }
    }
    
    // Save final image
    finalCanvas.SaveAs(filename);
    std::cout << "Parallel volume rendering complete. Output saved as " << filename << std::endl;
    
  } else {
    // Other ranks just send their data to rank 0
    MPI_Send(localColorData.data(), totalPixels * 4, MPI_FLOAT, 0, 0, MPI_COMM_WORLD);
  }
}

int main(int argc, char *argv[]) {
  // Initialize MPI
  MPI_Init(&argc, &argv);
  
  int rank, numRanks;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &numRanks);

  // Initialize Viskores
  viskores::cont::Initialize(argc, argv, viskores::cont::InitializeOptions::Strict);
  
  // Camera position for visibility calculations
  viskores::Vec3f_64 cameraPos = viskores::make_Vec(1.5, 1.5, 1.5);
  
  // Get rank-specific AMR data and bounds
  viskores::Vec3f_64 minBounds, maxBounds;
  auto [amrDataSet, fieldName] = getMPIPhysicsCodeProxyData(rank, numRanks, minBounds, maxBounds);

  // Calculate maximum distance from camera for visibility ordering
  double maxDistance = calculateMaxDistanceFromCamera(cameraPos, minBounds, maxBounds);
  
  // Gather all rank distances to determine visibility ordering
  std::vector<double> allMaxDistances(numRanks);
  MPI_Allgather(&maxDistance, 1, MPI_DOUBLE, allMaxDistances.data(), 1, MPI_DOUBLE, MPI_COMM_WORLD);
  
  // Create rank ordering based on distance (back-to-front, so farthest first)
  std::vector<std::pair<int, double>> orderedRanks;
  for (int i = 0; i < numRanks; ++i) {
    orderedRanks.emplace_back(i, allMaxDistances[i]);
  }
  
  // Sort back-to-front (largest distance first)
  std::sort(orderedRanks.begin(), orderedRanks.end(), 
            [](const auto& a, const auto& b) {
              return a.second > b.second;
            });

  if (rank == 0) {
    std::cout << "Visibility ordering (back-to-front):" << std::endl;
    for (const auto& [rankId, distance] : orderedRanks) {
      std::cout << "  Rank " << rankId << " (distance: " << distance << ")" << std::endl;
    }
  }

  // Set up a camera for rendering the input data
  viskores::rendering::Camera camera;
  camera.SetLookAt(viskores::Vec3f_32(0.5, 0.5, 0.5));
  camera.SetViewUp(viskores::make_Vec(0.f, 1.f, 0.f));
  camera.SetClippingRange(1.f, 10.f);
  camera.SetFieldOfView(60.f);
  camera.SetPosition(viskores::Vec3f_32(cameraPos[0], cameraPos[1], cameraPos[2]));

  // Set up color table and opacity
  viskores::cont::ColorTable colorTable("inferno");
  colorTable.AddPointAlpha(0., 0.);
  colorTable.AddPointAlpha(0.5, 0.2);
  colorTable.AddPointAlpha(1.0, 0.2);

  // Set up Actor for this rank's data
  viskores::rendering::Actor actor(amrDataSet, fieldName, fieldName, colorTable);
  viskores::rendering::Scene scene;
  scene.AddActor(actor);

  // Render the input data using OSMesa
  viskores::rendering::Color bg(0.2f, 0.2f, 0.2f, 1.0f);
  viskores::rendering::CanvasRayTracer canvas(1024, 1024);

  if (rank == 0) {
    std::cout << "Rendering with " << numRanks << " MPI ranks..." << std::endl;
  }

  // Render volume for this rank's data portion
  viskores::rendering::View3D viewVolume(
      scene, viskores::rendering::MapperVolume(), canvas, camera, bg);
  viewVolume.Paint();

  // Perform simple gather-to-rank-0 compositing
  gatherAndCompositeImages(canvas, orderedRanks, rank, numRanks, "volume_mpi.png");

  MPI_Finalize();
  return 0;
}