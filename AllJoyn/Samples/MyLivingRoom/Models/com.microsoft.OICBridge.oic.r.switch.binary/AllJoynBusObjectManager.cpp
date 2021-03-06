//-----------------------------------------------------------------------------
// <auto-generated> 
//   This code was generated by a tool. 
// 
//   Changes to this file may cause incorrect behavior and will be lost if  
//   the code is regenerated.
//
//   Tool: AllJoynCodeGenerator.exe
//
//   This tool is located in the Windows 10 SDK and the Windows 10 AllJoyn 
//   Visual Studio Extension in the Visual Studio Gallery.  
//
//   The generated code should be packaged in a Windows 10 C++/CX Runtime  
//   Component which can be consumed in any UWP-supported language using 
//   APIs that are available in Windows.Devices.AllJoyn.
//
//   Using AllJoynCodeGenerator - Invoke the following command with a valid 
//   Introspection XML file and a writable output directory:
//     AllJoynCodeGenerator -i <INPUT XML FILE> -o <OUTPUT DIRECTORY>
// </auto-generated>
//-----------------------------------------------------------------------------
#include "pch.h"

std::mutex AllJoynBusObjectManager::ModifyBusObjectMap;
std::map<alljoyn_busattachment, std::shared_ptr<std::map<std::string, std::tuple<alljoyn_busobject, bool, int>>>> AllJoynBusObjectManager::BusAttachmentMap;

QStatus AllJoynBusObjectManager::GetBusObject(_In_ const alljoyn_busattachment busAttachment, _In_ const PCSTR objectPath, _Out_ alljoyn_busobject* busObject)
{
    // Ensure thread safety when creating BusObjects so that for each BusAttachment there is
    // at most one BusObject with any given ObjectPath.
    std::unique_lock<std::mutex> lock(AllJoynBusObjectManager::ModifyBusObjectMap);

    if (AllJoynBusObjectManager::BusObjectExists(busAttachment, objectPath))
    {
        std::tuple<alljoyn_busobject, bool, int>& busObjectEntry = (*AllJoynBusObjectManager::BusAttachmentMap[busAttachment])[objectPath];
        *busObject = std::get<0>(busObjectEntry);

        // Increment the reference counter for this entry.
        std::get<2>(busObjectEntry)++;
        return ER_OK;
    }
    // If a matching BusObject does not exist, create and save one.
    RETURN_IF_QSTATUS_ERROR(AllJoynBusObjectManager::CreateBusObject(objectPath, busObject));
    return AllJoynBusObjectManager::SaveBusObject(busAttachment, objectPath, *busObject);
}

QStatus AllJoynBusObjectManager::ReleaseBusObject(_Inout_ alljoyn_busattachment busAttachment, _In_ const PCSTR objectPath)
{
    // Ensure thread safety when releasing BusObjects. Otherwise, BusObjects may continue to
    // exist and be registered on BusAttachments even when they have no references.
    std::unique_lock<std::mutex> lock(AllJoynBusObjectManager::ModifyBusObjectMap);

    if (AllJoynBusObjectManager::BusObjectExists(busAttachment, objectPath))
    {
        std::tuple<alljoyn_busobject, bool, int>& busObjectEntry = (*AllJoynBusObjectManager::BusAttachmentMap[busAttachment])[objectPath];

        // Decrement the reference counter for this entry.
        std::get<2>(busObjectEntry)--;

        // If there are no remaining references to this BusObject, unregister and destroy it.
        if (0 == std::get<2>(busObjectEntry))
        {
            return AllJoynBusObjectManager::UnregisterAndDestroyBusObject(busAttachment, objectPath);
        }
        return ER_OK;
    }
    return ER_OK;
}

QStatus AllJoynBusObjectManager::TryRegisterBusObject(_Inout_ alljoyn_busattachment busAttachment, _In_ const alljoyn_busobject busObject, _In_ const bool secure)
{
    PCSTR objectPath = alljoyn_busobject_getpath(busObject);

    // To register this BusObject, it must first be created using GetBusObject so that a reference exists in BusAttachmentMap.
    if (!AllJoynBusObjectManager::BusObjectExists(busAttachment, objectPath))
    {
        return ER_BUS_OBJ_NOT_FOUND;
    }

    if (!AllJoynBusObjectManager::BusObjectIsRegistered(busAttachment, busObject))
    {
        if (secure)
        {
            RETURN_IF_QSTATUS_ERROR(alljoyn_busattachment_registerbusobject_secure(busAttachment, busObject));
        }
        else
        {
            RETURN_IF_QSTATUS_ERROR(alljoyn_busattachment_registerbusobject(busAttachment, busObject));
        }
        std::tuple<alljoyn_busobject, bool, int>& busObjectEntry = (*AllJoynBusObjectManager::BusAttachmentMap[busAttachment])[objectPath];

        // Record that the BusObject in this entry has been registered with its associated BusAttachment.
        std::get<1>(busObjectEntry) = true;
        return ER_OK;
    }
    return ER_OK;
}

bool AllJoynBusObjectManager::BusObjectExists(_In_ const alljoyn_busattachment busAttachment, _In_ const PCSTR objectPath)
{
    // If a BusObject map exists for this BusAttachment, search for an entry with the specified ObjectPath.
    auto busObjectMapIterator = AllJoynBusObjectManager::BusAttachmentMap.find(busAttachment);
    if (AllJoynBusObjectManager::BusAttachmentMap.end() != busObjectMapIterator)
    {
        auto existingBusObject = busObjectMapIterator->second->find(std::string(objectPath));
        if (busObjectMapIterator->second->end() != existingBusObject)
        {
            return true;
        }
    }

    // If nothing is found above, the specified BusObject has not been saved yet.
    return false;
}

bool AllJoynBusObjectManager::BusObjectIsRegistered(_In_ const alljoyn_busattachment busAttachment, _In_ const alljoyn_busobject busObject)
{
    if (nullptr == busObject)
    {
        return false;
    }

    PCSTR objectPath = alljoyn_busobject_getpath(busObject);
    if (AllJoynBusObjectManager::BusObjectExists(busAttachment, objectPath))
    {
        std::tuple<alljoyn_busobject, bool, int> busObjectEntry = (*AllJoynBusObjectManager::BusAttachmentMap[busAttachment])[objectPath];
        return std::get<1>(busObjectEntry);
    }
    return false;
}

QStatus AllJoynBusObjectManager::CreateBusObject(_In_ const PCSTR objectPath, _Out_ alljoyn_busobject* busObject)
{
    alljoyn_busobject_callbacks callbacks =
    {
        nullptr,
        nullptr,
        nullptr,
        nullptr
    };
    alljoyn_busobject newBusObject = alljoyn_busobject_create(objectPath, false, &callbacks, nullptr);
    if (newBusObject == nullptr)
    {
        return ER_FAIL;
    }
    *busObject = newBusObject;
    return ER_OK;
}

QStatus AllJoynBusObjectManager::SaveBusObject(_In_ const alljoyn_busattachment busAttachment, _In_ const PCSTR objectPath, _In_ const alljoyn_busobject busObject)
{
    std::shared_ptr<std::map<std::string, std::tuple<alljoyn_busobject, bool, int>>> busObjectMap;

    // Create a BusObject entry for this BusAttachment if it does not exist.
    auto busObjectMapIterator = AllJoynBusObjectManager::BusAttachmentMap.find(busAttachment);
    if (AllJoynBusObjectManager::BusAttachmentMap.end() != busObjectMapIterator)
    {
        busObjectMap = busObjectMapIterator->second;
    }
    else
    {
        busObjectMap = std::shared_ptr<std::map<std::string, std::tuple<alljoyn_busobject, bool, int>>> (
            new std::map<std::string, std::tuple<alljoyn_busobject, bool, int>>);
        AllJoynBusObjectManager::BusAttachmentMap.insert(std::make_pair(busAttachment, busObjectMap));
    }

    // Insert this BusObject into busObjectMap if it does not exist.
    auto existingBusObject = busObjectMap->find(std::string(objectPath));
    if (busObjectMap->end() == existingBusObject)
    {
        // Initialize the BusObject entry as "not registered" and with a single reference.
        busObjectMap->insert(std::make_pair(std::string(objectPath), std::make_tuple(busObject, false, 1)));
    }
    return ER_OK;
}

QStatus AllJoynBusObjectManager::UnregisterAndDestroyBusObject(_Inout_ alljoyn_busattachment busAttachment, _In_ const PCSTR objectPath)
{
    if (AllJoynBusObjectManager::BusObjectExists(busAttachment, objectPath))
    {
        std::tuple<alljoyn_busobject, bool, int>& busObjectEntry = (*AllJoynBusObjectManager::BusAttachmentMap[busAttachment])[objectPath];

        // Unregister and destroy the specified BusObject, then remove its entry from the containing map.
        alljoyn_busattachment_unregisterbusobject(busAttachment, std::get<0>(busObjectEntry));
        alljoyn_busobject_destroy(std::get<0>(busObjectEntry));
        (*AllJoynBusObjectManager::BusAttachmentMap[busAttachment]).erase(objectPath);

        // If the associated BusAttachment contains no more BusObjects, also remove the BusAttachment from BusAttachmentMap.
        if (AllJoynBusObjectManager::BusAttachmentMap[busAttachment]->size() == 0)
        {
            AllJoynBusObjectManager::BusAttachmentMap.erase(busAttachment);
        }
    }
    return ER_OK;
}