// Copyright (C) Back Engineering Labs, Inc. - All Rights Reserved
//

#include <utils.h>

llvm::Expected<std::uint32_t>
sectionOffsetToRVA(std::uint32_t sectionNumber, std::uint32_t sectionOffset,
    const std::vector<llvm::object::coff_section>& sections) {
  if (sectionNumber > static_cast<int>(sections.size())) {
    return llvm::make_error<llvm::StringError>("Invalid section number",
                                               llvm::inconvertibleErrorCode());
  }
  const auto &section = sections[sectionNumber - 1];
  return section.VirtualAddress + sectionOffset;
}

bool isAddressInRange(const std::vector<Entry> &entries,
                      uint32_t targetAddress) {
  auto it = std::lower_bound(entries.begin(), entries.end(), targetAddress,
                             [](const Entry &entry, uint32_t address) {
                               return entry.rangeStart < address;
                             });

  if (it != entries.end() && it->rangeStart <= targetAddress &&
      targetAddress <= it->rangeEnd) {
    return true;
  }
  return false;
}

std::vector<Entry> parseEntriesFromFile(const std::string &filePath) {
  std::vector<Entry> entries;
  std::ifstream file(filePath);
  if (!file) {
    std::cerr << "Error: Failed to open file." << std::endl;
    return entries;
  }

  std::string line;
  std::getline(file, line); // Skip the header line

  while (std::getline(file, line)) {
    std::istringstream ss(line);
    std::string rangeStartStr, rangeEndStr, originalStr;

    if (std::getline(ss, rangeStartStr, ',') &&
        std::getline(ss, rangeEndStr, ',') && std::getline(ss, originalStr)) {
      Entry entry = {
          static_cast<uint32_t>(std::stoul(rangeStartStr, nullptr, 16)),
          static_cast<uint32_t>(std::stoul(rangeEndStr, nullptr, 16)),
          static_cast<uint32_t>(std::stoul(originalStr, nullptr, 16))};
      entries.push_back(entry);
    }
  }

  // Sort the entries by rangeStart to allow binary search
  std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
    return a.rangeStart < b.rangeStart;
  });

  return entries;
}

llvm::Error readModuleInfo(llvm::StringRef modulePath, ModuleInfo &info) {
  using namespace llvm;
  using namespace llvm::object;

  auto expectedBinary = createBinary(modulePath);
  if (!expectedBinary)
    return expectedBinary.takeError();

  OwningBinary<Binary> binary = std::move(*expectedBinary);

  if (binary.getBinary()->isCOFF()) {
    auto const obj = llvm::cast<COFFObjectFile>(binary.getBinary());
    for (auto const &sectionRef : obj->sections())
      info.sections.push_back(*obj->getCOFFSection(sectionRef));

    for (auto const &debugDir : obj->debug_directories()) {
      info.signature = debugDir.TimeDateStamp;
      if (debugDir.Type == IMAGE_DEBUG_TYPE_CODEVIEW) {
        llvm::codeview::DebugInfo const *debugInfo;
        StringRef pdbFileName;
        exitOnErr(obj->getDebugPDBInfo(&debugDir, debugInfo, pdbFileName));

        switch (debugInfo->Signature.CVSignature) {
        case OMF::Signature::PDB70:
          info.age = debugInfo->PDB70.Age;
          std::memcpy(&info.guid, debugInfo->PDB70.Signature,
                      sizeof(info.guid));
          break;
        }
      }
    }

    return Error::success();
  }

  return errorCodeToError(std::make_error_code(std::errc::not_supported));
}

llvm::Expected<SectionAndOffset>
rvaToSectionAndOffset(std::uint32_t rva,
                      const std::vector<llvm::object::coff_section> &sections) {
  for (size_t i = 0; i < sections.size(); ++i) {
    const auto &section = sections[i];
    if (rva >= section.VirtualAddress &&
        rva < (section.VirtualAddress + section.VirtualSize)) {

      std::uint32_t offset = rva - section.VirtualAddress;
      return SectionAndOffset{static_cast<uint32_t>(i + 1), offset};
    }
  }

  // If no section contains the RVA, return an error
  return llvm::make_error<llvm::StringError>(
      llvm::formatv("RVA {0:x} is not within any section!", rva),
      llvm::inconvertibleErrorCode());
}
