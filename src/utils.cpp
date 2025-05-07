// Copyright (C) Back Engineering Labs, Inc. - All Rights Reserved
//

#include <utils.h>

#define INVALID_ADDRESS 0xFFFFFFFF

llvm::Expected<std::uint32_t>
sectionOffsetToRVA(std::uint32_t sectionNumber, std::uint32_t sectionOffset,
                   const std::vector<llvm::object::coff_section> &sections) {
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
  std::ifstream file(filePath, std::ios::binary | std::ios::ate);
  if (!file) {
    std::cerr << "Error: Failed to open file.\n";
    return entries;
  }

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  if (size % sizeof(Entry) != 0) {
    std::cerr << "Error: File size is not a multiple of Entry size ("
              << sizeof(Entry) << " bytes).\n";
    return entries;
  }

  std::vector<uint8_t> buffer(size);
  if (!file.read(reinterpret_cast<char *>(buffer.data()), size)) {
    std::cerr << "Error: Failed to read file into buffer.\n";
    return entries;
  }

  std::size_t count = size / sizeof(Entry);
  const Entry *data = reinterpret_cast<const Entry *>(buffer.data());

  entries.assign(data, data + count);
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
