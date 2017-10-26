// Copyright � 1996-2005, Valve Corporation, All rights reserved.
//
// Purpose: static link master include

#ifndef SYSTEM_H
#define SYSTEM_H

#define UID_PREFIX generated_id_
#define UID_CAT1(a, c) a##c
#define UID_CAT2(a, c) UID_CAT1(a, c)
#define EXPAND_CONCAT(a, c) UID_CAT1(a, c)
#define UNIQUE_ID UID_CAT2(UID_PREFIX, __LINE__)

// helper atom macros - force preprocessor symbol expansion
#define SYMBOL_TO_STRING(token1) #token1
#define EXPAND_SYMBOL_TO_STRING(token1) SYMBOL_TO_STRING(token1)
#define EXPAND_SYMBOL(token) token

#if defined(_STATIC_LINKED)
// for platforms built with static linking, the dll interface gets spoofed

// Contains each published subsystem's 'CreateInterface'
typedef void *(*createFn)(const char *pName, int *pReturnCode);
class DynamicLibraryList {
 public:
  DynamicLibraryList(const char *subSystemName, createFn createFunction);

  const char *m_subSystemName;
  createFn m_createFn;
  DynamicLibraryList *m_next;
  static DynamicLibraryList *s_DynamicLibraryList;
};

// creates the unique dll subsystem class symbol
// class constructor handles the list population
#define MAKE_DLL_CLASS(subSystem)                             \
  static DynamicLibraryList __g_##subSystem##_DynamicLibrary( \
      EXPAND_SYMBOL_TO_STRING(_SUBSYSTEM), CreateInterface);

#if defined(_SUBSYSTEM)
#define PUBLISH_DLL_SUBSYSTEM() MAKE_DLL_CLASS(_SUBSYSTEM)
#else
// must define _SUBSYSTEM
#define PUBLISH_DLL_SUBSYSTEM() Project error... Missing _SUBSYSTEM = <name>
#endif

#endif

#if !defined(_STATIC_LINKED) && !defined(PUBLISH_DLL_SUBSYSTEM)
// for platforms built with dynamic linking, the dll interface does not need
// spoofing
#define PUBLISH_DLL_SUBSYSTEM()
#endif

#if defined(_STATIC_LINKED)
#define PRIVATE static
#else
#define PRIVATE
#endif

#define MAKE_NAME_UNIQUE(identifier) \
  EXPAND_CONCAT(EXPAND_CONCAT(_SUBSYSTEM, _), identifier)

// the low tech solution
#if defined(_STATIC_LINKED)

#define ActivityDataOps MAKE_NAME_UNIQUE(ActivityDataOps)
#define ActivityList_Free MAKE_NAME_UNIQUE(ActivityList_Free)
#define ActivityList_IndexForName MAKE_NAME_UNIQUE(ActivityList_IndexForName)
#define ActivityList_Init MAKE_NAME_UNIQUE(ActivityList_Init)
#define ActivityList_NameForIndex MAKE_NAME_UNIQUE(ActivityList_NameForIndex)
#define ActivityList_RegisterPrivateActivity \
  MAKE_NAME_UNIQUE(ActivityList_RegisterPrivateActivity)
#define ActivityList_RegisterSharedActivities \
  MAKE_NAME_UNIQUE(ActivityList_RegisterSharedActivities)
#define ActivityList_RegisterSharedActivity \
  MAKE_NAME_UNIQUE(ActivityList_RegisterSharedActivity)
#define activitylist_t MAKE_NAME_UNIQUE(activitylist_t)
#define AddSurfacepropFile MAKE_NAME_UNIQUE(AddSurfacepropFile)
#define AllocateStringHelper MAKE_NAME_UNIQUE(AllocateStringHelper)
#define AllocateStringHelper2 MAKE_NAME_UNIQUE(AllocateStringHelper2)
#define AllocateUniqueDataTableName \
  MAKE_NAME_UNIQUE(AllocateUniqueDataTableName)
#define andomVector MAKE_NAME_UNIQUE(andomVector)
#define ApplyMultiDamage MAKE_NAME_UNIQUE(ApplyMultiDamage)
#define BlendBones MAKE_NAME_UNIQUE(BlendBones)
#define BreakModelList MAKE_NAME_UNIQUE(BreakModelList)
#define BuildAllAnimationEventIndexes \
  MAKE_NAME_UNIQUE(BuildAllAnimationEventIndexes)
#define BuildBoneChain MAKE_NAME_UNIQUE(BuildBoneChain)
#define CActivityDataOps MAKE_NAME_UNIQUE(CActivityDataOps)
#define CalcBoneAdj MAKE_NAME_UNIQUE(CalcBoneAdj)
#define CalcBoneDerivatives MAKE_NAME_UNIQUE(CalcBoneDerivatives)
#define CalcBonePosition MAKE_NAME_UNIQUE(CalcBonePosition)
#define CalcBoneQuaternion MAKE_NAME_UNIQUE(CalcBoneQuaternion)
#define CalcBoneVelocityFromDerivative \
  MAKE_NAME_UNIQUE(CalcBoneVelocityFromDerivative)
#define CalcPoseSingle MAKE_NAME_UNIQUE(CalcPoseSingle)
#define CalcProceduralBone MAKE_NAME_UNIQUE(CalcProceduralBone)
#define CalcRopeStartingConditions MAKE_NAME_UNIQUE(CalcRopeStartingConditions)
#define CAmmoDef MAKE_NAME_UNIQUE(CAmmoDef)
#define CAutoGameSystem MAKE_NAME_UNIQUE(CAutoGameSystem)
#define CAutoGameSystemPerFrame MAKE_NAME_UNIQUE(CAutoGameSystemPerFrame)
#define CBaseEntityList MAKE_NAME_UNIQUE(CBaseEntityList)
#define CBaseGameSystem MAKE_NAME_UNIQUE(CBaseGameSystem)
#define CBaseGameSystemPerFrame MAKE_NAME_UNIQUE(CBaseGameSystemPerFrame)
#define CBaseHandle MAKE_NAME_UNIQUE(CBaseHandle)
#define CBasePanel MAKE_NAME_UNIQUE(CBasePanel)
#define CBasePlayerAnimState MAKE_NAME_UNIQUE(CBasePlayerAnimState)
#define CBaseRopePhysics MAKE_NAME_UNIQUE(CBaseRopePhysics)
#define CBoneCache MAKE_NAME_UNIQUE(CBoneCache)
#define CCollisionEvent MAKE_NAME_UNIQUE(CCollisionEvent)
#define CCollisionProperty MAKE_NAME_UNIQUE(CCollisionProperty)
#define CCopyRecipientFilter MAKE_NAME_UNIQUE(CCopyRecipientFilter)
#define CPASFilter MAKE_NAME_UNIQUE(CPASFilter)
#define CPVSFilter MAKE_NAME_UNIQUE(CPVSFilter)
#define CPASAttenuationFilter MAKE_NAME_UNIQUE(CPASAttenuationFilter)
#define CDataObjectAccessSystem MAKE_NAME_UNIQUE(CDataObjectAccessSystem)
#define CDecalEmitterSystem MAKE_NAME_UNIQUE(CDecalEmitterSystem)
#define CDirtySpatialPartitionEntityList \
  MAKE_NAME_UNIQUE(CDirtySpatialPartitionEntityList)
#define CEntInfo MAKE_NAME_UNIQUE(CEntInfo)
#define CEntityMapData MAKE_NAME_UNIQUE(CEntityMapData)
#define CEntitySaveRestoreBlockHandler \
  MAKE_NAME_UNIQUE(CEntitySaveRestoreBlockHandler)
#define CEntitySaveUtils MAKE_NAME_UNIQUE(CEntitySaveUtils)
#define CEntitySphereQuery MAKE_NAME_UNIQUE(CEntitySphereQuery)
#define CEnvHeadcrabCanisterShared MAKE_NAME_UNIQUE(CEnvHeadcrabCanisterShared)
#define CEnvWindShared MAKE_NAME_UNIQUE(CEnvWindShared)
#define CFlaggedEntitiesEnum MAKE_NAME_UNIQUE(CFlaggedEntitiesEnum)
#define CFlexSceneFileManager MAKE_NAME_UNIQUE(CFlexSceneFileManager)
#define CGameMovement MAKE_NAME_UNIQUE(CGameMovement)
#define CGameRulesRegister MAKE_NAME_UNIQUE(CGameRulesRegister)
#define CGameSaveRestoreInfo MAKE_NAME_UNIQUE(CGameSaveRestoreInfo)
#define CGameStringPool MAKE_NAME_UNIQUE(CGameStringPool)
#define CGameTrace MAKE_NAME_UNIQUE(CGameTrace)
#define CGameUI MAKE_NAME_UNIQUE(CGameUI)
#define CGameWeaponManager MAKE_NAME_UNIQUE(CGameWeaponManager)
#define CHL2GameMovement MAKE_NAME_UNIQUE(CHL2GameMovement)
#define CIKContext MAKE_NAME_UNIQUE(CIKContext)
#define CIKTarget MAKE_NAME_UNIQUE(CIKTarget)
#define CIterativeSheetSimulator MAKE_NAME_UNIQUE(CIterativeSheetSimulator)
#define ClearMultiDamage MAKE_NAME_UNIQUE(ClearMultiDamage)
#define CMessage MAKE_NAME_UNIQUE(CMessage)
#define CMultiDamage MAKE_NAME_UNIQUE(CMultiDamage)
#define CObjectsFileLoad MAKE_NAME_UNIQUE(CObjectsFileLoad)
#define ComputeSurroundingBox MAKE_NAME_UNIQUE(ComputeSurroundingBox)
#define CountdownTimer MAKE_NAME_UNIQUE(CountdownTimer)
#define CPhysicsGameTrace MAKE_NAME_UNIQUE(CPhysicsGameTrace)
#define CPhysicsSpring MAKE_NAME_UNIQUE(CPhysicsSpring)
#define CPhysObjSaveRestoreOps MAKE_NAME_UNIQUE(CPhysObjSaveRestoreOps)
#define CPhysSaveRestoreBlockHandler \
  MAKE_NAME_UNIQUE(CPhysSaveRestoreBlockHandler)
#define CPlayerLocalData MAKE_NAME_UNIQUE(CPlayerLocalData)
#define CPlayerState MAKE_NAME_UNIQUE(CPlayerState)
#define CPositionWatcherList MAKE_NAME_UNIQUE(CPositionWatcherList)
#define CPrecacheRegister MAKE_NAME_UNIQUE(CPrecacheRegister)
#define CPredictableId MAKE_NAME_UNIQUE(CPredictableId)
#define CPredictableList MAKE_NAME_UNIQUE(CPredictableList)
#define CPropData MAKE_NAME_UNIQUE(CPropData)
#define CRagdollLowViolenceManager MAKE_NAME_UNIQUE(CRagdollLowViolenceManager)
#define CRagdollLRURetirement MAKE_NAME_UNIQUE(CRagdollLRURetirement)
#define CreateInterface MAKE_NAME_UNIQUE(CreateInterface)
#define CRestore MAKE_NAME_UNIQUE(CRestore)
#define CSave MAKE_NAME_UNIQUE(CSave)
#define CSaveRestoreBlockSet MAKE_NAME_UNIQUE(CSaveRestoreBlockSet)
#define CSaveRestoreData MAKE_NAME_UNIQUE(CSaveRestoreData)
#define CSaveRestoreSegment MAKE_NAME_UNIQUE(CSaveRestoreSegment)
#define CSceneTokenProcessor MAKE_NAME_UNIQUE(CSceneTokenProcessor)
#define CSheetSimulator MAKE_NAME_UNIQUE(CSheetSimulator)
#define CSimplePhysics MAKE_NAME_UNIQUE(CSimplePhysics)
#define CSolidSetDefaults MAKE_NAME_UNIQUE(CSolidSetDefaults)
#define CSoundControllerImp MAKE_NAME_UNIQUE(CSoundControllerImp)
#define CSoundEmitterSystem MAKE_NAME_UNIQUE(CSoundEmitterSystem)
#define CSoundEmitterSystemBase MAKE_NAME_UNIQUE(CSoundEmitterSystemBase)
#define CSoundEnvelope MAKE_NAME_UNIQUE(CSoundEnvelope)
#define CSoundEnvelopeController MAKE_NAME_UNIQUE(CSoundEnvelopeController)
#define CSoundPatch MAKE_NAME_UNIQUE(CSoundPatch)
#define CSoundPatchSaveRestoreOps MAKE_NAME_UNIQUE(CSoundPatchSaveRestoreOps)
#define CStudioBoneCache MAKE_NAME_UNIQUE(CStudioBoneCache)
#define CStudioHdr MAKE_NAME_UNIQUE(CStudioHdr)
#define CTakeDamageInfo MAKE_NAME_UNIQUE(CTakeDamageInfo)
#define CTraceFilterEntity MAKE_NAME_UNIQUE(CTraceFilterEntity)
#define CTraceFilterEntityIgnoreOther \
  MAKE_NAME_UNIQUE(CTraceFilterEntityIgnoreOther)
#define CTraceFilterLOS MAKE_NAME_UNIQUE(CTraceFilterLOS)
#define CTraceFilterNoNPCsOrPlayer MAKE_NAME_UNIQUE(CTraceFilterNoNPCsOrPlayer)
#define CTraceFilterOnlyNPCsAndPlayer \
  MAKE_NAME_UNIQUE(CTraceFilterOnlyNPCsAndPlayer)
#define CTraceFilterSimple MAKE_NAME_UNIQUE(CTraceFilterSimple)
#define CTraceFilterSimpleList MAKE_NAME_UNIQUE(CTraceFilterSimpleList)
#define CTraceFilterSkipNPCs MAKE_NAME_UNIQUE(CTraceFilterSkipNPCs)
#define CTraceFilterSkipTwoEntities \
  MAKE_NAME_UNIQUE(CTraceFilterSkipTwoEntities)
#define CurrentViewOrigin MAKE_NAME_UNIQUE(CurrentViewOrigin)
#define CurrentViewForward MAKE_NAME_UNIQUE(CurrentViewForward)
#define CurrentViewRight MAKE_NAME_UNIQUE(CurrentViewRight)
#define CurrentViewUp MAKE_NAME_UNIQUE(CurrentViewUp)
#define CUserMessages MAKE_NAME_UNIQUE(CUserMessages)
#define cvar MAKE_NAME_UNIQUE(cvar)
#define datacache MAKE_NAME_UNIQUE(datacache)
#define DataTableRecvProxy_LengthProxy \
  MAKE_NAME_UNIQUE(DataTableRecvProxy_LengthProxy)
#define DebugDrawLine MAKE_NAME_UNIQUE(DebugDrawLine)
#define debugoverlay MAKE_NAME_UNIQUE(debugoverlay)
#define decalsystem MAKE_NAME_UNIQUE(decalsystem)
#define DispatchEffect MAKE_NAME_UNIQUE(DispatchEffect)
#define DoAxisInterpBone MAKE_NAME_UNIQUE(DoAxisInterpBone)
#define DoQuatInterpBone MAKE_NAME_UNIQUE(DoQuatInterpBone)
#define engine MAKE_NAME_UNIQUE(engine)
#define engineCache MAKE_NAME_UNIQUE(engineCache)
#define enginesound MAKE_NAME_UNIQUE(enginesound)
#define enginetrace MAKE_NAME_UNIQUE(enginetrace)
#define enginevgui MAKE_NAME_UNIQUE(enginevgui)
#define EntityFromEntityHandle MAKE_NAME_UNIQUE(EntityFromEntityHandle)
#define EntityParticleTrailInfo_t MAKE_NAME_UNIQUE(EntityParticleTrailInfo_t)
#define entitytable_t MAKE_NAME_UNIQUE(entitytable_t)
#define EventList_AddEventEntry MAKE_NAME_UNIQUE(EventList_AddEventEntry)
#define EventList_Free MAKE_NAME_UNIQUE(EventList_Free)
#define EventList_GetEventType MAKE_NAME_UNIQUE(EventList_GetEventType)
#define EventList_IndexForName MAKE_NAME_UNIQUE(EventList_IndexForName)
#define EventList_Init MAKE_NAME_UNIQUE(EventList_Init)
#define EventList_NameForIndex MAKE_NAME_UNIQUE(EventList_NameForIndex)
#define EventList_RegisterPrivateEvent \
  MAKE_NAME_UNIQUE(EventList_RegisterPrivateEvent)
#define EventList_RegisterSharedEvent \
  MAKE_NAME_UNIQUE(EventList_RegisterSharedEvent)
#define EventList_RegisterSharedEvents \
  MAKE_NAME_UNIQUE(EventList_RegisterSharedEvents)
#define ExtractAnimValue MAKE_NAME_UNIQUE(ExtractAnimValue)
#define ExtractBbox MAKE_NAME_UNIQUE(ExtractBbox)
#define FactoryList_Retrieve MAKE_NAME_UNIQUE(FactoryList_Retrieve)
#define FactoryList_Store MAKE_NAME_UNIQUE(FactoryList_Store)
#define FileSystem_LoadModule MAKE_NAME_UNIQUE(FileSystem_LoadModule)
#define FileSystem_Shutdown MAKE_NAME_UNIQUE(FileSystem_Shutdown)
#define FileSystem_UnloadModule MAKE_NAME_UNIQUE(FileSystem_UnloadModule)
#define FileWeaponInfo_t MAKE_NAME_UNIQUE(FileWeaponInfo_t)
#define FindBodygroupByName MAKE_NAME_UNIQUE(FindBodygroupByName)
#define FindHitboxSetByName MAKE_NAME_UNIQUE(FindHitboxSetByName)
#define FindTransitionSequence MAKE_NAME_UNIQUE(FindTransitionSequence)
#define fluidevent_t MAKE_NAME_UNIQUE(fluidevent_t)
#define g_ActivityStrings MAKE_NAME_UNIQUE(g_ActivityStrings)
#define g_bMovementOptimizations MAKE_NAME_UNIQUE(g_bMovementOptimizations)
#define g_bTextMode MAKE_NAME_UNIQUE(g_bTextMode)
#define g_bUsedWeaponSlots MAKE_NAME_UNIQUE(g_bUsedWeaponSlots)
#define g_EntityCollisionHash MAKE_NAME_UNIQUE(g_EntityCollisionHash)
#define g_EventList MAKE_NAME_UNIQUE(g_EventList)
#define g_EventStrings MAKE_NAME_UNIQUE(g_EventStrings)
#define g_FileSystemFactory MAKE_NAME_UNIQUE(g_FileSystemFactory)
#define g_flLastBodyPitch MAKE_NAME_UNIQUE(g_flLastBodyPitch)
#define g_flLastBodyYaw MAKE_NAME_UNIQUE(g_flLastBodyYaw)
#define g_lateralBob MAKE_NAME_UNIQUE(g_lateralBob)
#define g_nActivityListVersion MAKE_NAME_UNIQUE(g_nActivityListVersion)
#define g_nEventListVersion MAKE_NAME_UNIQUE(g_nEventListVersion)
#define g_pDataCache MAKE_NAME_UNIQUE(g_pDataCache)
#define g_pEffects MAKE_NAME_UNIQUE(g_pEffects)
#define g_pFileSystem MAKE_NAME_UNIQUE(g_pFileSystem)
#define g_pGameMovement MAKE_NAME_UNIQUE(g_pGameMovement)
#define g_pGameSaveRestoreBlockSet MAKE_NAME_UNIQUE(g_pGameSaveRestoreBlockSet)
#define g_PhysDefaultObjectParams MAKE_NAME_UNIQUE(g_PhysDefaultObjectParams)
#define g_PhysGameTrace MAKE_NAME_UNIQUE(g_PhysGameTrace)
#define g_PhysObjSaveRestoreOps MAKE_NAME_UNIQUE(g_PhysObjSaveRestoreOps)
#define g_PhysSaveRestoreBlockHandler \
  MAKE_NAME_UNIQUE(g_PhysSaveRestoreBlockHandler)
#define g_PhysWorldObject MAKE_NAME_UNIQUE(g_PhysWorldObject)
#define g_pMaterialSystemHardwareConfig \
  MAKE_NAME_UNIQUE(g_pMaterialSystemHardwareConfig)
#define g_pMatSystemSurface MAKE_NAME_UNIQUE(g_pMatSystemSurface)
#define g_pMDLCache MAKE_NAME_UNIQUE(g_pMDLCache)
#define g_pModelNameLaser MAKE_NAME_UNIQUE(g_pModelNameLaser)
#define g_pMoveData MAKE_NAME_UNIQUE(g_pMoveData)
#define g_pPhysSaveRestoreManager MAKE_NAME_UNIQUE(g_pPhysSaveRestoreManager)
#define g_pPredictionSystems MAKE_NAME_UNIQUE(g_pPredictionSystems)
#define g_pShaderUtil MAKE_NAME_UNIQUE(g_pShaderUtil)
#define g_pStringTableClientSideChoreoScenes \
  MAKE_NAME_UNIQUE(g_pStringTableClientSideChoreoScenes)
#define g_pStringTableInfoPanel MAKE_NAME_UNIQUE(g_pStringTableInfoPanel)
#define g_pStringTableMaterials MAKE_NAME_UNIQUE(g_pStringTableMaterials)
#define g_sModelIndexBloodDrop MAKE_NAME_UNIQUE(g_sModelIndexBloodDrop)
#define g_sModelIndexBloodSpray MAKE_NAME_UNIQUE(g_sModelIndexBloodSpray)
#define g_sModelIndexBubbles MAKE_NAME_UNIQUE(g_sModelIndexBubbles)
#define g_sModelIndexFireball MAKE_NAME_UNIQUE(g_sModelIndexFireball)
#define g_sModelIndexLaser MAKE_NAME_UNIQUE(g_sModelIndexLaser)
#define g_sModelIndexLaserDot MAKE_NAME_UNIQUE(g_sModelIndexLaserDot)
#define g_sModelIndexSmoke MAKE_NAME_UNIQUE(g_sModelIndexSmoke)
#define g_sModelIndexWExplosion MAKE_NAME_UNIQUE(g_sModelIndexWExplosion)
#define g_SolidSetup MAKE_NAME_UNIQUE(g_SolidSetup)
#define g_StringTableGameRules MAKE_NAME_UNIQUE(g_StringTableGameRules)
#define g_verticalBob MAKE_NAME_UNIQUE(g_verticalBob)
#define gameeventmanager MAKE_NAME_UNIQUE(gameeventmanager)
#define GameStringSystem MAKE_NAME_UNIQUE(GameStringSystem)
#define gameuifuncs MAKE_NAME_UNIQUE(gameuifuncs)
#define GetAnimationEvent MAKE_NAME_UNIQUE(GetAnimationEvent)
#define GetAttachmentLocalSpace MAKE_NAME_UNIQUE(GetAttachmentLocalSpace)
#define GetBodygroup MAKE_NAME_UNIQUE(GetBodygroup)
#define GetBodygroupCount MAKE_NAME_UNIQUE(GetBodygroupCount)
#define GetBodygroupName MAKE_NAME_UNIQUE(GetBodygroupName)
#define GetEntitySaveRestoreBlockHandler \
  MAKE_NAME_UNIQUE(GetEntitySaveRestoreBlockHandler)
#define GetEntitySaveUtils MAKE_NAME_UNIQUE(GetEntitySaveUtils)
#define GetEventIndexForSequence MAKE_NAME_UNIQUE(GetEventIndexForSequence)
#define GetEyePosition MAKE_NAME_UNIQUE(GetEyePosition)
#define GetHitboxSetCount MAKE_NAME_UNIQUE(GetHitboxSetCount)
#define GetHitboxSetName MAKE_NAME_UNIQUE(GetHitboxSetName)
#define GetInvalidWeaponInfoHandle MAKE_NAME_UNIQUE(GetInvalidWeaponInfoHandle)
#define GetMaterialIndex MAKE_NAME_UNIQUE(GetMaterialIndex)
#define GetMaterialNameFromIndex MAKE_NAME_UNIQUE(GetMaterialNameFromIndex)
#define GetNumBodyGroups MAKE_NAME_UNIQUE(GetNumBodyGroups)
#define GetPhysObjSaveRestoreOps MAKE_NAME_UNIQUE(GetPhysObjSaveRestoreOps)
#define GetPhysSaveRestoreBlockHandler \
  MAKE_NAME_UNIQUE(GetPhysSaveRestoreBlockHandler)
#define GetSequenceActivity MAKE_NAME_UNIQUE(GetSequenceActivity)
#define GetSequenceActivityName MAKE_NAME_UNIQUE(GetSequenceActivityName)
#define GetSequenceFlags MAKE_NAME_UNIQUE(GetSequenceFlags)
#define GetSequenceLinearMotion MAKE_NAME_UNIQUE(GetSequenceLinearMotion)
#define GetSequenceName MAKE_NAME_UNIQUE(GetSequenceName)
#define GetSoundSaveRestoreOps MAKE_NAME_UNIQUE(GetSoundSaveRestoreOps)
#define GetWindspeedAtTime MAKE_NAME_UNIQUE(GetWindspeedAtTime)
#define groundlinksallocated MAKE_NAME_UNIQUE(groundlinksallocated)
#define HasAnimationEventOfType MAKE_NAME_UNIQUE(HasAnimationEventOfType)
#define IGameSystem MAKE_NAME_UNIQUE(IGameSystem)
#define IGameSystemPerFrame MAKE_NAME_UNIQUE(IGameSystemPerFrame)
#define ik MAKE_NAME_UNIQUE(ik)
#define IMoveHelper MAKE_NAME_UNIQUE(IMoveHelper)
#define ImpulseScale MAKE_NAME_UNIQUE(ImpulseScale)
#define IndexModelSequences MAKE_NAME_UNIQUE(IndexModelSequences)
#define InitPose MAKE_NAME_UNIQUE(InitPose)
#define InterfaceReg MAKE_NAME_UNIQUE(InterfaceReg)
#define IntervalDistance MAKE_NAME_UNIQUE(IntervalDistance)
#define IntervalTimer MAKE_NAME_UNIQUE(IntervalTimer)
#define IsInPrediction MAKE_NAME_UNIQUE(IsInPrediction)
#define IsValidEntityPointer MAKE_NAME_UNIQUE(IsValidEntityPointer)
#define linksallocated MAKE_NAME_UNIQUE(linksallocated)
#define LookupActivity MAKE_NAME_UNIQUE(LookupActivity)
#define LookupSequence MAKE_NAME_UNIQUE(LookupSequence)
#define LookupWeaponInfoSlot MAKE_NAME_UNIQUE(LookupWeaponInfoSlot)
#define m_flLastMoveYaw MAKE_NAME_UNIQUE(m_flLastMoveYaw)
#define MainViewOrigin MAKE_NAME_UNIQUE(MainViewOrigin)
#define MainViewForward MAKE_NAME_UNIQUE(MainViewForward)
#define MainViewRight MAKE_NAME_UNIQUE(MainViewRight)
#define MainViewUp MAKE_NAME_UNIQUE(MainViewUp)
#if !defined(_SHARED_LIB)
#define materials MAKE_NAME_UNIQUE(materials)
#else
#define materials \
  VguiMatSurface_materials  // shared lib has no materials of own
#endif
#define mdlcache MAKE_NAME_UNIQUE(mdlcache)
#define modelinfo MAKE_NAME_UNIQUE(modelinfo)
#define modelrender MAKE_NAME_UNIQUE(modelrender)
#define mstudioanimdesc_t MAKE_NAME_UNIQUE(mstudioanimdesc_t)
#define mstudiomodel_t MAKE_NAME_UNIQUE(mstudiomodel_t)
#define NDebugOverlay MAKE_NAME_UNIQUE(NDebugOverlay)
#define networkstringtable MAKE_NAME_UNIQUE(networkstringtable)
#define nexttoken MAKE_NAME_UNIQUE(nexttoken)
#define partition MAKE_NAME_UNIQUE(partition)
#define PassServerEntityFilter MAKE_NAME_UNIQUE(PassServerEntityFilter)
#define PhysBlockHeader_t MAKE_NAME_UNIQUE(PhysBlockHeader_t)
#define physcollision MAKE_NAME_UNIQUE(physcollision)
#define PhysComputeSlideDirection MAKE_NAME_UNIQUE(PhysComputeSlideDirection)
#define PhysCreateBbox MAKE_NAME_UNIQUE(PhysCreateBbox)
#define PhysDisableEntityCollisions \
  MAKE_NAME_UNIQUE(PhysDisableEntityCollisions)
#define PhysDisableObjectCollisions \
  MAKE_NAME_UNIQUE(PhysDisableObjectCollisions)
#define PhysEnableEntityCollisions MAKE_NAME_UNIQUE(PhysEnableEntityCollisions)
#define PhysEnableObjectCollisions MAKE_NAME_UNIQUE(PhysEnableObjectCollisions)
#define physenv MAKE_NAME_UNIQUE(physenv)
#define PhysForceClearVelocity MAKE_NAME_UNIQUE(PhysForceClearVelocity)
#define PhysFrictionEffect MAKE_NAME_UNIQUE(PhysFrictionEffect)
#define physgametrace MAKE_NAME_UNIQUE(physgametrace)
#define PhysGetDefaultAABBSolid MAKE_NAME_UNIQUE(PhysGetDefaultAABBSolid)
#define PhysHasContactWithOtherInDirection \
  MAKE_NAME_UNIQUE(PhysHasContactWithOtherInDirection)
#define physics MAKE_NAME_UNIQUE(physics)
#define PhysicsGameSystem MAKE_NAME_UNIQUE(PhysicsGameSystem)
#define physicssound MAKE_NAME_UNIQUE(physicssound)
#define PhysObjectHeader_t MAKE_NAME_UNIQUE(PhysObjectHeader_t)
#define PhysParseSurfaceData MAKE_NAME_UNIQUE(PhysParseSurfaceData)
#define physprops MAKE_NAME_UNIQUE(physprops)
#define PhysRecheckObjectPair MAKE_NAME_UNIQUE(PhysRecheckObjectPair)
#define PrecacheFileWeaponInfoDatabase \
  MAKE_NAME_UNIQUE(PrecacheFileWeaponInfoDatabase)
#define PrecacheMaterial MAKE_NAME_UNIQUE(PrecacheMaterial)
#define predictables MAKE_NAME_UNIQUE(predictables)
#define QuaternionAccumulate MAKE_NAME_UNIQUE(QuaternionAccumulate)
#define QuaternionMA MAKE_NAME_UNIQUE(QuaternionMA)
#define QuaternionSM MAKE_NAME_UNIQUE(QuaternionSM)
#define RagdollActivate MAKE_NAME_UNIQUE(RagdollActivate)
#define RagdollApplyAnimationAsVelocity \
  MAKE_NAME_UNIQUE(RagdollApplyAnimationAsVelocity)
#define RagdollComputeExactBbox MAKE_NAME_UNIQUE(RagdollComputeExactBbox)
#define RagdollCreate MAKE_NAME_UNIQUE(RagdollCreate)
#define RagdollDestroy MAKE_NAME_UNIQUE(RagdollDestroy)
#define RagdollExtractBoneIndices MAKE_NAME_UNIQUE(RagdollExtractBoneIndices)
#define RagdollGetBoneMatrix MAKE_NAME_UNIQUE(RagdollGetBoneMatrix)
#define RagdollIsAsleep MAKE_NAME_UNIQUE(RagdollIsAsleep)
#define RagdollSetupAnimatedFriction \
  MAKE_NAME_UNIQUE(RagdollSetupAnimatedFriction)
#define RagdollSetupCollisions MAKE_NAME_UNIQUE(RagdollSetupCollisions)
#define CopyPackedAnimatedFriction MAKE_NAME_UNIQUE(CopyPackedAnimatedFriction)
#define random MAKE_NAME_UNIQUE(random)
#define RandomInterval MAKE_NAME_UNIQUE(RandomInterval)
#define ReadEncryptedKVFile MAKE_NAME_UNIQUE(ReadEncryptedKVFile)
#define ReadInterval MAKE_NAME_UNIQUE(ReadInterval)
#define ReadUsercmd MAKE_NAME_UNIQUE(ReadUsercmd)
#define ReadWeaponDataFromFileForSlot \
  MAKE_NAME_UNIQUE(ReadWeaponDataFromFileForSlot)
#define RecvPropUtlVector MAKE_NAME_UNIQUE(RecvPropUtlVector)
#define RecvProxy_UtlVectorElement MAKE_NAME_UNIQUE(RecvProxy_UtlVectorElement)
#define RecvProxy_UtlVectorElement_DataTable \
  MAKE_NAME_UNIQUE(RecvProxy_UtlVectorElement_DataTable)
#define RecvProxy_UtlVectorLength MAKE_NAME_UNIQUE(RecvProxy_UtlVectorLength)
#define RegisterUserMessages MAKE_NAME_UNIQUE(RegisterUserMessages)
#define RemapAngleRange MAKE_NAME_UNIQUE(RemapAngleRange)
#define ResetActivityIndexes MAKE_NAME_UNIQUE(ResetActivityIndexes)
#define ResetEventIndexes MAKE_NAME_UNIQUE(ResetEventIndexes)
#define ResetWindspeed MAKE_NAME_UNIQUE(ResetWindspeed)
#define s_pInterfaceRegs MAKE_NAME_UNIQUE(s_pInterfaceRegs)
#define SaveInit MAKE_NAME_UNIQUE(SaveInit)
#define SaveRestoreBlockHeader_t MAKE_NAME_UNIQUE(SaveRestoreBlockHeader_t)
#define ScaleBones MAKE_NAME_UNIQUE(ScaleBones)
#define Scene_Printf MAKE_NAME_UNIQUE(Scene_Printf)
#define SelectHeaviestSequence MAKE_NAME_UNIQUE(SelectHeaviestSequence)
#define SelectWeightedSequence MAKE_NAME_UNIQUE(SelectWeightedSequence)
#define SendPropUtlVector MAKE_NAME_UNIQUE(SendPropUtlVector)
#define SendProxy_LengthTable MAKE_NAME_UNIQUE(SendProxy_LengthTable)
#define SendProxy_UtlVectorElement MAKE_NAME_UNIQUE(SendProxy_UtlVectorElement)
#define SendProxy_UtlVectorElement_DataTable \
  MAKE_NAME_UNIQUE(SendProxy_UtlVectorElement_DataTable)
#define SendProxy_UtlVectorLength MAKE_NAME_UNIQUE(SendProxy_UtlVectorLength)
#define SENTENCEG_Lookup MAKE_NAME_UNIQUE(SENTENCEG_Lookup)
#define SetActivityForSequence MAKE_NAME_UNIQUE(SetActivityForSequence)
#define SetBodygroup MAKE_NAME_UNIQUE(SetBodygroup)
#define SetEventIndexForSequence MAKE_NAME_UNIQUE(SetEventIndexForSequence)
#define SetupSingleBoneMatrix MAKE_NAME_UNIQUE(SetupSingleBoneMatrix)
#define SharedRandomAngle MAKE_NAME_UNIQUE(SharedRandomAngle)
#define SharedRandomFloat MAKE_NAME_UNIQUE(SharedRandomFloat)
#define SharedRandomInt MAKE_NAME_UNIQUE(SharedRandomInt)
#define SharedRandomVector MAKE_NAME_UNIQUE(SharedRandomVector)
#define SlerpBones MAKE_NAME_UNIQUE(SlerpBones)
#define SolveBone MAKE_NAME_UNIQUE(SolveBone)
#define SoundCommand_t MAKE_NAME_UNIQUE(SoundCommand_t)
#define soundemitterbase MAKE_NAME_UNIQUE(soundemitterbase)
#define SpawnBlood MAKE_NAME_UNIQUE(SpawnBlood)
#define StandardFilterRules MAKE_NAME_UNIQUE(StandardFilterRules)
#define Studio_AlignIKMatrix MAKE_NAME_UNIQUE(Studio_AlignIKMatrix)
#define Studio_AnimMovement MAKE_NAME_UNIQUE(Studio_AnimMovement)
#define Studio_AnimPosition MAKE_NAME_UNIQUE(Studio_AnimPosition)
#define Studio_AnimVelocity MAKE_NAME_UNIQUE(Studio_AnimVelocity)
#define Studio_BoneIndexByName MAKE_NAME_UNIQUE(Studio_BoneIndexByName)
#define Studio_BuildMatrices MAKE_NAME_UNIQUE(Studio_BuildMatrices)
#define Studio_CalcBoneToBoneTransform \
  MAKE_NAME_UNIQUE(Studio_CalcBoneToBoneTransform)
#define Studio_CPS MAKE_NAME_UNIQUE(Studio_CPS)
#define Studio_CreateBoneCache MAKE_NAME_UNIQUE(Studio_CreateBoneCache)
#define Studio_DestroyBoneCache MAKE_NAME_UNIQUE(Studio_DestroyBoneCache)
#define Studio_Duration MAKE_NAME_UNIQUE(Studio_Duration)
#define Studio_FindAnimDistance MAKE_NAME_UNIQUE(Studio_FindAnimDistance)
#define Studio_FindAttachment MAKE_NAME_UNIQUE(Studio_FindAttachment)
#define Studio_FindRandomAttachment \
  MAKE_NAME_UNIQUE(Studio_FindRandomAttachment)
#define Studio_FindSeqDistance MAKE_NAME_UNIQUE(Studio_FindSeqDistance)
#define Studio_FPS MAKE_NAME_UNIQUE(Studio_FPS)
#define Studio_GetController MAKE_NAME_UNIQUE(Studio_GetController)
#define Studio_GetDefaultSurfaceProps \
  MAKE_NAME_UNIQUE(Studio_GetDefaultSurfaceProps)
#define Studio_GetKeyValueText MAKE_NAME_UNIQUE(Studio_GetKeyValueText)
#define Studio_GetMass MAKE_NAME_UNIQUE(Studio_GetMass)
#define Studio_GetPoseParameter MAKE_NAME_UNIQUE(Studio_GetPoseParameter)
#define Studio_IKRuleWeight MAKE_NAME_UNIQUE(Studio_IKRuleWeight)
#define Studio_IKShouldLatch MAKE_NAME_UNIQUE(Studio_IKShouldLatch)
#define Studio_IKTail MAKE_NAME_UNIQUE(Studio_IKTail)
#define Studio_InvalidateBoneCache MAKE_NAME_UNIQUE(Studio_InvalidateBoneCache)
#define Studio_LocalPoseParameter MAKE_NAME_UNIQUE(Studio_LocalPoseParameter)
#define Studio_MaxFrame MAKE_NAME_UNIQUE(Studio_MaxFrame)
#define Studio_SeqMovement MAKE_NAME_UNIQUE(Studio_SeqMovement)
#define Studio_SeqVelocity MAKE_NAME_UNIQUE(Studio_SeqVelocity)
#define Studio_SetController MAKE_NAME_UNIQUE(Studio_SetController)
#define Studio_SetPoseParameter MAKE_NAME_UNIQUE(Studio_SetPoseParameter)
#define Studio_SolveIK MAKE_NAME_UNIQUE(Studio_SolveIK)
#define studiohdr_t MAKE_NAME_UNIQUE(studiohdr_t)
#define SURFACEPROP_MANIFEST_FILE MAKE_NAME_UNIQUE(SURFACEPROP_MANIFEST_FILE)
#define Sys_GetFactory MAKE_NAME_UNIQUE(Sys_GetFactory)
#define Sys_GetFactoryThis MAKE_NAME_UNIQUE(Sys_GetFactoryThis)
#define Sys_LoadInterface MAKE_NAME_UNIQUE(Sys_LoadInterface)
#define Sys_LoadModule MAKE_NAME_UNIQUE(Sys_LoadModule)
#define Sys_UnloadModule MAKE_NAME_UNIQUE(Sys_UnloadModule)
#define te MAKE_NAME_UNIQUE(te)
#define TE_ArmorRicochet MAKE_NAME_UNIQUE(TE_ArmorRicochet)
#define TE_BeamEntPoint MAKE_NAME_UNIQUE(TE_BeamEntPoint)
#define TE_BeamEnts MAKE_NAME_UNIQUE(TE_BeamEnts)
#define TE_BeamFollow MAKE_NAME_UNIQUE(TE_BeamFollow)
#define TE_BeamLaser MAKE_NAME_UNIQUE(TE_BeamLaser)
#define TE_BeamPoints MAKE_NAME_UNIQUE(TE_BeamPoints)
#define TE_BeamRing MAKE_NAME_UNIQUE(TE_BeamRing)
#define TE_BeamRingPoint MAKE_NAME_UNIQUE(TE_BeamRingPoint)
#define TE_BeamSpline MAKE_NAME_UNIQUE(TE_BeamSpline)
#define TE_BloodSprite MAKE_NAME_UNIQUE(TE_BloodSprite)
#define TE_BloodStream MAKE_NAME_UNIQUE(TE_BloodStream)
#define TE_BreakModel MAKE_NAME_UNIQUE(TE_BreakModel)
#define TE_BSPDecal MAKE_NAME_UNIQUE(TE_BSPDecal)
#define TE_Bubbles MAKE_NAME_UNIQUE(TE_Bubbles)
#define TE_BubbleTrail MAKE_NAME_UNIQUE(TE_BubbleTrail)
#define TE_Decal MAKE_NAME_UNIQUE(TE_Decal)
#define TE_DispatchEffect MAKE_NAME_UNIQUE(TE_DispatchEffect)
#define TE_Dust MAKE_NAME_UNIQUE(TE_Dust)
#define TE_DynamicLight MAKE_NAME_UNIQUE(TE_DynamicLight)
#define TE_EnergySplash MAKE_NAME_UNIQUE(TE_EnergySplash)
#define TE_Explosion MAKE_NAME_UNIQUE(TE_Explosion)
#define TE_FootprintDecal MAKE_NAME_UNIQUE(TE_FootprintDecal)
#define TE_GaussExplosion MAKE_NAME_UNIQUE(TE_GaussExplosion)
#define TE_GlowSprite MAKE_NAME_UNIQUE(TE_GlowSprite)
#define TE_KillPlayerAttachments MAKE_NAME_UNIQUE(TE_KillPlayerAttachments)
#define TE_LargeFunnel MAKE_NAME_UNIQUE(TE_LargeFunnel)
#define TE_MetalSparks MAKE_NAME_UNIQUE(TE_MetalSparks)
#define TE_MuzzleFlash MAKE_NAME_UNIQUE(TE_MuzzleFlash)
#define TE_PlayerDecal MAKE_NAME_UNIQUE(TE_PlayerDecal)
#define TE_ProjectDecal MAKE_NAME_UNIQUE(TE_ProjectDecal)
#define TE_ShatterSurface MAKE_NAME_UNIQUE(TE_ShatterSurface)
#define TE_ShowLine MAKE_NAME_UNIQUE(TE_ShowLine)
#define TE_Smoke MAKE_NAME_UNIQUE(TE_Smoke)
#define TE_Sparks MAKE_NAME_UNIQUE(TE_Sparks)
#define TE_Sprite MAKE_NAME_UNIQUE(TE_Sprite)
#define TE_SpriteSpray MAKE_NAME_UNIQUE(TE_SpriteSpray)
#define TE_WorldDecal MAKE_NAME_UNIQUE(TE_WorldDecal)
#define TempCreateInterface MAKE_NAME_UNIQUE(TempCreateInterface)
#define touchevent_t MAKE_NAME_UNIQUE(touchevent_t)
#define touchlink_t MAKE_NAME_UNIQUE(touchlink_t)
#define UTIL_AngleDiff MAKE_NAME_UNIQUE(UTIL_AngleDiff)
#define UTIL_BloodDrips MAKE_NAME_UNIQUE(UTIL_BloodDrips)
#define UTIL_BloodImpact MAKE_NAME_UNIQUE(UTIL_BloodImpact)
#define UTIL_Bubbles MAKE_NAME_UNIQUE(UTIL_Bubbles)
#define UTIL_EmitAmbientSound MAKE_NAME_UNIQUE(UTIL_EmitAmbientSound)
#define UTIL_FreeFile MAKE_NAME_UNIQUE(UTIL_FreeFile)
#define UTIL_FunctionFromName MAKE_NAME_UNIQUE(UTIL_FunctionFromName)
#define UTIL_FunctionToName MAKE_NAME_UNIQUE(UTIL_FunctionToName)
#define UTIL_IsLowViolence MAKE_NAME_UNIQUE(UTIL_IsLowViolence)
#define UTIL_LoadActivityRemapFile MAKE_NAME_UNIQUE(UTIL_LoadActivityRemapFile)
#define UTIL_LoadFileForMe MAKE_NAME_UNIQUE(UTIL_LoadFileForMe)
#define UTIL_PrecacheDecal MAKE_NAME_UNIQUE(UTIL_PrecacheDecal)
#define UTIL_ScreenShake MAKE_NAME_UNIQUE(UTIL_ScreenShake)
#define UTIL_ShouldShowBlood MAKE_NAME_UNIQUE(UTIL_ShouldShowBlood)
#define UTIL_Smoke MAKE_NAME_UNIQUE(UTIL_Smoke)
#define UTIL_StringToColor32 MAKE_NAME_UNIQUE(UTIL_StringToColor32)
#define UTIL_StringToFloatArray MAKE_NAME_UNIQUE(UTIL_StringToFloatArray)
#define UTIL_StringToIntArray MAKE_NAME_UNIQUE(UTIL_StringToIntArray)
#define UTIL_StringToVector MAKE_NAME_UNIQUE(UTIL_StringToVector)
#define UTIL_Tracer MAKE_NAME_UNIQUE(UTIL_Tracer)
#define UTIL_TranslateSoundName MAKE_NAME_UNIQUE(UTIL_TranslateSoundName)
#define UTIL_VecToPitch MAKE_NAME_UNIQUE(UTIL_VecToPitch)
#define UTIL_VecToYaw MAKE_NAME_UNIQUE(UTIL_VecToYaw)
#define UTIL_WaterLevel MAKE_NAME_UNIQUE(UTIL_WaterLevel)
#define UTIL_YawToVector MAKE_NAME_UNIQUE(UTIL_YawToVector)
#define VerifySequenceIndex MAKE_NAME_UNIQUE(VerifySequenceIndex)
#define VGui_CreateGlobalPanels MAKE_NAME_UNIQUE(VGui_CreateGlobalPanels)
#define VGui_PostInit MAKE_NAME_UNIQUE(VGui_PostInit)
#define VGui_Shutdown MAKE_NAME_UNIQUE(VGui_Shutdown)
#define VGui_Startup MAKE_NAME_UNIQUE(VGui_Startup)
#define W_Precache MAKE_NAME_UNIQUE(W_Precache)
#define WriteUsercmd MAKE_NAME_UNIQUE(WriteUsercmd)

#endif

#if defined(_STATIC_LINKED) && defined(_VGUI_DLL)
// unique these overloaded symbols to avoid static linking clash
// ensures locality to vguidll lib
#define scheme vguidll_scheme
#define surface vguidll_surface
#define system vguidll_system
#define ivgui vguidll_ivgui
#define filesystem vguidll_filesystem
#define localize vguidll_localize
#define ipanel vguidll_ipanel
#endif

#endif
