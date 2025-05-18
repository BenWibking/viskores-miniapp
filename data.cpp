#include <viskores/cont/PartitionedDataSet.h>
#include <viskores/cont/MergePartitionedDataSet.h>
#include <viskores/filter/entity_extraction/Threshold.h>
#include <viskores/source/Amr.h>

auto getPhysicsCodeProxyData() -> std::tuple<viskores::cont::PartitionedDataSet, std::string> {
  // Generate AMR dataset
  const int nx = 8;
  const int nlevels = 3;
  viskores::source::Amr amrDataGenerator{};
  amrDataGenerator.SetDimension(3); // 3D
  amrDataGenerator.SetCellsPerDimension(nx);
  amrDataGenerator.SetNumberOfLevels(nlevels);
  viskores::cont::PartitionedDataSet const amrData = amrDataGenerator.Execute();

  // Remove covered cells on coarse AMR levels
  viskores::filter::entity_extraction::Threshold threshold;
  threshold.SetLowerThreshold(0);
  threshold.SetUpperThreshold(1);
  threshold.SetActiveField(viskores::cont::GetGlobalGhostCellFieldName());
  viskores::cont::PartitionedDataSet derivedDataSet =
      threshold.Execute(amrData);
  
  std::string fieldName = "RTDataCells";
  return std::make_tuple(derivedDataSet, fieldName);
}
