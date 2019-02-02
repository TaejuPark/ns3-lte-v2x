/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Nicola Baldo  <nbaldo@cttc.es>
 * Author: Marco Miozzo <mmiozzo@cttc.es>
 * Modified by: NIST // Contributions may not be subject to US copyright.
 */

#ifndef LTE_UE_MAC_ENTITY_H
#define LTE_UE_MAC_ENTITY_H



#include <map>

#include <ns3/lte-mac-sap.h>
#include <ns3/lte-ue-cmac-sap.h>
#include <ns3/lte-ue-phy-sap.h>
#include <ns3/lte-amc.h>
#include <ns3/nstime.h>
#include <ns3/event-id.h>
#include <vector>
#include <ns3/packet.h>
#include <ns3/packet-burst.h>
#include "ns3/traced-value.h"
#include "ns3/trace-source-accessor.h"


namespace ns3 {

class UniformRandomVariable;

class LteUeMac :   public Object
{
  /// allow UeMemberLteUeCmacSapProvider class friend access
  friend class UeMemberLteUeCmacSapProvider;
  /// allow UeMemberLteMacSapProvider class friend access
  friend class UeMemberLteMacSapProvider;
  /// allow UeMemberLteUePhySapUser class friend access
  friend class UeMemberLteUePhySapUser;

public:
  /**
   * \brief Get the type ID.
   * \return The object TypeId
   */
  static TypeId GetTypeId (void);

  LteUeMac ();
  virtual ~LteUeMac ();
  virtual void DoDispose (void);

  /**
  * \brief Get the LTE MAC SAP provider
  * \return a pointer to the LTE MAC SAP provider
  */
  LteMacSapProvider*  GetLteMacSapProvider (void);
  /**
  * \brief Set the LTE UE CMAC SAP user
  * \param s The LTE UE CMAC SAP User
  */
  void  SetLteUeCmacSapUser (LteUeCmacSapUser* s);
  /**
  * \brief Get the LTE CMAC SAP provider
  * \return a pointer to the LTE CMAC SAP provider
  */
  LteUeCmacSapProvider*  GetLteUeCmacSapProvider (void);
  
  /**
  * \brief Set the component carried ID
  * \param index The component carrier ID
  */
  void SetComponentCarrierId (uint8_t index);
  void SetUEID (uint32_t ueid);

  /**
  * \brief Get the PHY SAP user
  * \return A pointer to the SAP user of the PHY
  */
  LteUePhySapUser* GetLteUePhySapUser ();

  /**
  * \brief Set the PHY SAP Provider
  * \param s A pointer to the PHY SAP Provider
  */
  void SetLteUePhySapProvider (LteUePhySapProvider* s);
  
  /**
  * \brief Forwarded from LteUePhySapUser: trigger the start from a new frame
  *
  * \param frameNo The frame number
  * \param subframeNo The subframe number
  */
  void DoSubframeIndication (uint32_t frameNo, uint32_t subframeNo);

 /**
  * Assign a fixed random variable stream number to the random variables
  * used by this model.  Return the number of streams (possibly zero) that
  * have been assigned.
  *
  * \param stream The first stream index to use
  * \return The number of stream indices assigned by this model
  */
  int64_t AssignStreams (int64_t stream);

  /**
   *\brief Get the discovery Rx pools
   *
   * \return list of discovery reception pools
   */
  std::list< Ptr<SidelinkRxDiscResourcePool> > GetDiscRxPools ();

  /**
   *\brief Get the discovery Tx pool
   *
   * \return pointer to the discovery transmission pool
   */
  Ptr<SidelinkTxDiscResourcePool> GetDiscTxPool ();

  /**
   * TracedCallback signature for transmission of discovery message.
   *
   * \param [in] rnti The RNTI of the UE
   * \param [in] proSeAppCode The ProSe application code
   */
  typedef void (* DiscoveryAnnouncementTracedCallback)
    (const uint16_t rnti, const uint32_t proSeAppCode);

private:
  // forwarded from MAC SAP
  /**
   * Transmit PDU function
   *
   * \param params LteMacSapProvider::TransmitPduParameters
   */
  void DoTransmitPdu (LteMacSapProvider::TransmitPduParameters params);
  /**
   * Report buffers status function
   *
   * \param params LteMacSapProvider::ReportBufferStatusParameters
   */
  void DoReportBufferStatus (LteMacSapProvider::ReportBufferStatusParameters params);

  // forwarded from UE CMAC SAP
  /**
   * Configure RACH function
   *
   * \param rc LteUeCmacSapProvider::RachConfig
   */
  void DoConfigureRach (LteUeCmacSapProvider::RachConfig rc);
  /**
   * Start contention based random access procedure function
   */
   void DoStartContentionBasedRandomAccessProcedure ();
  /**
   * Set RNTI
   *
   * \param rnti The RNTI of the UE
   */
  void DoSetRnti (uint16_t rnti);
  /**
   * Start non contention based random access procedure function
   *
   * \param rnti The RNTI
   * \param rapId The RAPID
   * \param prachMask The PRACH mask
   */
  void DoStartNonContentionBasedRandomAccessProcedure (uint16_t rnti, uint8_t rapId, uint8_t prachMask);
  /**
   * Add LC function
   *
   * \param lcId The logical channel id
   * \param lcConfig The logical channel config
   * \param msu The corresponding LteMacSapUser
   */
  void DoAddLc (uint8_t lcId, LteUeCmacSapProvider::LogicalChannelConfig lcConfig, LteMacSapUser* msu);
  /**
   * Remove LC function
   *
   * \param lcId The logical channel id
   */
  void DoRemoveLc (uint8_t lcId);
  /**
   * Reset function
   */
  void DoReset ();
  /**
   * Adds a new Logical Channel (LC) used for Sidelink
   *
   * \param lcId The ID of the logical channel
   * \param srcL2Id The L2 group id of the source
   * \param dstL2Id The L2 group id of the destination
   * \param lcConfig The LC configuration provided by the RRC
   * \param msu The corresponding LteMacSapUser
   */
  void DoAddSlLc (uint8_t lcId, uint32_t srcL2Id, uint32_t dstL2Id, LteUeCmacSapProvider::LogicalChannelConfig lcConfig, LteMacSapUser* msu);
  /**
   * Removes an existing Sidelink LC
   *
   * \param lcId The LC ID
   * \param srcL2Id The L2 group id of the source
   * \param dstL2Id The L2 group id of the destination
   */
  void DoRemoveSlLc (uint8_t lcId, uint32_t srcL2Id, uint32_t dstL2Id);

  //Sidelink communication

  /**
   * Add Sidelink communication transmission pool function
   * Adds transmission pool for Sidelink communication
   *
   * \param dstL2Id The destination Layer 2 group ID
   * \param pool The pointer to the SidelinkTxCommResourcePool
   */
  void DoAddSlCommTxPool (uint32_t dstL2Id, Ptr<SidelinkTxCommResourcePool> pool);
  /**
   * Remove Sidelink communication transmission pool function
   * Removes transmission pool for Sidelink communication
   *
   * \param dstL2Id The destination Layer 2 group ID
   */
  void DoRemoveSlCommTxPool (uint32_t dstL2Id);
  /**
   * Set Sidelink communication pool function
   * Sets reception pools for Sidelink communication
   *
   * \param pools The list of reception pools for Sidelink communication
   */
  void DoSetSlCommRxPools (std::list<Ptr<SidelinkRxCommResourcePool> > pools);
  /**
   * Add Sidelink destination function
   * Adds a new destination for Sidelink communication to listen
   *
   * \param destination The destination Layer 2 group ID
   */
  void DoAddSlDestination (uint32_t destination);
  /**
   * Remove Sidelink destination function
   * Removes a destination from the list destinations of Sidelink communication
   *
   * \param destination The destination Layer 2 group ID
   */
  void DoRemoveSlDestination (uint32_t destination);

   //Sidelink discovery

  /**
   * Set Sidelink discovery transmission pool function
   * Sets transmission pool for Sidelink discovery
   *
   * \param pool The pointer to the SidelinkTxDiscResourcePool
   */
  void DoSetSlDiscTxPool (Ptr<SidelinkTxDiscResourcePool> pool);
  /**
   * Remove Sidelink discovery transmission pool function
   * Removes transmission pool for Sidelink discovery
   */
  void DoRemoveSlDiscTxPool ();
  /**
   * Set Sidelink discovery reception pool function
   * Sets reception pool for Sidelink discovery
   *
   * \param pools The pointer to the SidelinkRxDiscResourcePool
   */
  void DoSetSlDiscRxPools (std::list<Ptr<SidelinkRxDiscResourcePool> > pools);
  /**
   * Modify Sidelink discovery transmission applications function
   * Modifies/Sets the Sidelink discovery transmission applications
   *
   * \param apps The list of Sidelink discovery transmission applications
   */
  void DoModifyDiscTxApps (std::list<uint32_t> apps);
  /**
   * Modify Sidelink discovery reception applications function
   * Modifies/Sets the Sidelink discovery reception applications
   *
   * \param apps The list of Sidelink discovery reception applications
   */
  void DoModifyDiscRxApps (std::list<uint32_t> apps);

  // forwarded from PHY SAP
  /**
   * Receive Phy PDU function
   *
   * \param p The packet
   */
  void DoReceivePhyPdu (Ptr<Packet> p);
  /**
   * Receive LTE control message function
   *
   * \param msg The LTE control message
   */
  void DoReceiveLteControlMessage (Ptr<LteControlMessage> msg);
  /**
   * Notify change of timing function
   * The PHY notifies the change of timing as consequence of a change of SyncRef, the MAC adjust its timing
   *
   * \param frameNo The current PHY frame number
   * \param subframeNo The current PHY subframe number
   */
  void DoNotifyChangeOfTiming (uint32_t frameNo, uint32_t subframeNo);
  /**
   * Notify Sidelink enabled function
   * The PHY notifies the MAC the Sidelink is activated
   */
  void DoNotifySidelinkEnabled ();
  
  // internal methods
  /**
   * Randomly select and send RA preamble function
   */
  void RandomlySelectAndSendRaPreamble ();
 /**
  * Send RA preamble function
  *
  * \param contention if true randomly select and send the RA preamble
  */
  void SendRaPreamble (bool contention);
  /**
   * Start waiting for RA response function
   */
  void StartWaitingForRaResponse ();
  /**
   * Receive the RA response function
   *
   * \param raResponse The random access response received
   */
  void RecvRaResponse (BuildRarListElement_s raResponse);
 /**
  * RA response timeout function
  *
  * \param contention if true randomly select and send the RA preamble
  */
  void RaResponseTimeout (bool contention);
  /**
   * Send report buffer status
   */
  void SendReportBufferStatus (void);
  /**
   * Send Sidelink report buffer status
   */
  void SendSidelinkReportBufferStatus (void);
  /**
   * Refresh HARQ processes packet buffer function
   */
  void RefreshHarqProcessesPacketBuffer (void);

  /// component carrier Id --> used to address sap
  uint8_t m_componentCarrierId;

  bool m_v2v;
  bool m_first;
  std::vector<bool> m_not_sensed_subframe;

private:

  /// LcInfo structure
  struct LcInfo
  {
    LteUeCmacSapProvider::LogicalChannelConfig lcConfig; ///< logical channel config
    LteMacSapUser* macSapUser; ///< MAC SAP user
  };

  std::map <uint8_t, LcInfo> m_lcInfoMap; ///< logical channel info map

  LteMacSapProvider* m_macSapProvider; ///< MAC SAP provider

  LteUeCmacSapUser* m_cmacSapUser; ///< CMAC SAP user
  LteUeCmacSapProvider* m_cmacSapProvider; ///< CMAC SAP provider

  LteUePhySapProvider* m_uePhySapProvider; ///< UE Phy SAP provider
  LteUePhySapUser* m_uePhySapUser; ///< UE Phy SAP user
  
  std::map <uint8_t, LteMacSapProvider::ReportBufferStatusParameters> m_ulBsrReceived; ///< BSR received from RLC (the last one)
  
  
  Time m_bsrPeriodicity; ///< BSR periodicity
  Time m_bsrLast; ///< BSR last
  
  bool m_freshUlBsr; ///< true when a BSR has been received in the last TTI

  uint8_t m_harqProcessId; ///< HARQ process ID
  std::vector < Ptr<PacketBurst> > m_miUlHarqProcessesPacket; ///< Packets under transmission of the UL HARQ processes
  std::vector < uint8_t > m_miUlHarqProcessesPacketTimer; ///< timer for packet life in the buffer

  uint16_t m_rnti; ///< RNTI
  uint32_t m_ueid;

  bool m_rachConfigured; ///< is RACH configured?
  LteUeCmacSapProvider::RachConfig m_rachConfig; ///< RACH configuration
  uint8_t m_raPreambleId; ///< RA preamble ID
  uint8_t m_preambleTransmissionCounter; ///< preamble transmission counter
  uint16_t m_backoffParameter; ///< backoff parameter
  EventId m_noRaResponseReceivedEvent; ///< no RA response received event ID
  Ptr<UniformRandomVariable> m_raPreambleUniformVariable; ///< RA preamble random variable

  uint32_t m_frameNo; ///< frame number
  uint32_t m_subframeNo; ///< subframe number
  uint8_t m_raRnti; ///< RA RNTI
  bool m_waitingForRaResponse; ///< waiting for RA response

  /// Sidelink Communication related variables
  struct SidelinkLcIdentifier
  {
    uint8_t lcId; ///< Sidelink LCID
    uint32_t srcL2Id; ///< Source L2 group ID
    uint32_t dstL2Id; ///< Destination L2 group ID
  };

  /**
   * Less than operator
   *
   * \param l first SidelinkLcIdentifier
   * \param r second SidelinkLcIdentifier
   * \returns true if first SidelinkLcIdentifier parameter values are less than the second SidelinkLcIdentifier parameters"
   */
  friend bool operator < (const SidelinkLcIdentifier &l, const SidelinkLcIdentifier &r)
  {
    return l.lcId < r.lcId || (l.lcId == r.lcId && l.srcL2Id < r.srcL2Id) || (l.lcId == r.lcId && l.srcL2Id == r.srcL2Id && l.dstL2Id < r.dstL2Id);
  }

  struct SidelinkGrantV2V
  {
    uint8_t m_subChannelIndex;
    SidelinkCommResourcePool::SubframeInfo m_grantedSubframe;
    uint8_t m_rbStart;
    uint8_t m_rbLen;
    uint8_t m_mcs;
    uint32_t m_tbSize;
  };

  /// Sidelink grant related variables
  struct SidelinkGrant
  {
    //fields common with SL_DCI
    uint16_t m_resPscch; ///< Resource for PSCCH
    uint8_t m_tpc; ///< TPC
    uint8_t m_hopping; ///< hopping flag
    uint8_t m_rbStart; ///< models RB assignment
    uint8_t m_rbLen; ///< models RB assignment
    uint8_t m_hoppingInfo; ///< models RB assignment when hopping is enabled
    uint8_t m_iTrp; ///< Index of Time recourse pattern (TRP)

    //other fields
    uint8_t m_mcs; ///< Modulation and Coding Scheme
    uint32_t m_tbSize; ///< Transport Block Size
  };

  /// Sidelink communication pool information
  struct PoolInfo
  {
    Ptr<SidelinkCommResourcePool> m_pool; ///< The Sidelink communication resource pool
    SidelinkCommResourcePool::SubframeInfo m_currentScPeriod; ///< Start of the current Sidelink Control (SC) period
    SidelinkGrant m_currentGrant; ///< Grant for the next SC period
    SidelinkGrantV2V m_currentGrantV2V;
    SidelinkCommResourcePool::SubframeInfo m_nextScPeriod; ///< Start of next SC period

    uint32_t m_npscch; ///< Number of PSCCH available in the pool

    bool m_grantReceived; ///< True if we receive the grant
    SidelinkGrant m_nextGrant; ///< Grant received for the next SC period
    SidelinkGrantV2V m_nextGrantV2V;
    SidelinkGrantV2V m_prevGrantV2V;

    uint32_t m_reserveCount;
    uint32_t m_chosenSubframe;

    std::list<SidelinkCommResourcePool::SidelinkTransmissionInfo> m_pscchTx; ///< List of PSCCH transmissions within the pool
    std::list<SidelinkCommResourcePool::SidelinkTransmissionInfo> m_psschTx; ///< List of PSSCH transmissions within the pool

    Ptr<PacketBurst> m_miSlHarqProcessPacket; ///< Packets under transmission of the SL HARQ process
  };

  std::map <SidelinkLcIdentifier, LcInfo> m_slLcInfoMap; ///< Sidelink logical channel info map
  Time m_slBsrPeriodicity; ///< Sidelink buffer status report periodicity
  Time m_slBsrLast; ///< Time of last transmitted Sidelink buffer status report
  bool m_freshSlBsr; ///< true when a BSR has been received in the last TTI
  std::map <SidelinkLcIdentifier, LteMacSapProvider::ReportBufferStatusParameters> m_slBsrReceived; ///< Sidelink BSR received from RLC (the last one)

  std::map <uint32_t, PoolInfo > m_sidelinkTxPoolsMap; ///< Map of Sidelink Tx pools with destination L2 group ID as its key
  std::list <Ptr<SidelinkRxCommResourcePool> > m_sidelinkRxPools; ///< List of Sidelink communication reception pools
  std::list <uint32_t> m_sidelinkDestinations; ///< List of Sidelink communication destinations

  Ptr<LteAmc> m_amc; ///< Pointer to LteAmc class; needed now since UE is doing scheduling
  Ptr<UniformRandomVariable> m_ueSelectedUniformVariable;  ///<  A uniform random variable used to choose random resources, RB start
                                                           ///<  and iTrp values in UE selected mode
  //fields for fixed UE_SELECTED pools
  uint8_t m_slKtrp; ///< Number of active resource blocks in the TRP used.
  uint8_t m_setTrpIndex; ///< TRP index to be used
  bool m_useSetTrpIndex; ///< True if TRP index set, i.e., m_setTrpIndex to be used
  uint8_t m_slGrantMcs; ///< Sidelink grant MCS
  uint8_t m_slGrantSize; ///< The number of RBs allocated per UE for Sidelink

  /// Sidelink discovery grant related variables
  struct DiscGrant
  {
    uint16_t m_rnti; ///< RNTI of the UE
    uint8_t m_resPsdch; ///< A randomly chosen resource index from the PSDCH resource pool
  };

  /// Sidelink discovery pool information
  struct DiscPoolInfo
  {
    Ptr<SidelinkTxDiscResourcePool> m_pool; ///< The Sidelink discovery transmission pool
    SidelinkDiscResourcePool::SubframeInfo m_currentDiscPeriod; ///< Start of the current discovery period
    DiscGrant m_currentGrant; ///< Grant for the next discovery period
    SidelinkDiscResourcePool::SubframeInfo m_nextDiscPeriod; ///< Start of next discovery period

    uint32_t m_npsdch; ///< Number of PSDCH available in the pool

    bool m_grantReceived; ///< True if UE received the grant
    DiscGrant m_nextGrant; ///< Grant received for the next discovery period

    std::list<SidelinkDiscResourcePool::SidelinkTransmissionInfo> m_psdchTx; ///< List of PSDCH transmissions within the pool
  };

  DiscPoolInfo m_discTxPool; ///< Sidelink discovery transmission pool
  std::list <Ptr<SidelinkRxDiscResourcePool> > m_discRxPools; ///< Sidelink discovery reception pool

  std::list<uint32_t> m_discTxApps; ///< List of Sidelink discovery transmission applications
  std::list<uint32_t> m_discRxApps; ///< List of Sidelink discovery reception applications

  Ptr<UniformRandomVariable> m_p1UniformVariable; ///< A uniform random variable to compare with the Tx probability of UE selected pool
  Ptr<UniformRandomVariable> m_resUniformVariable;///< A uniform random variable to randomly choose the resource index from the PSDCH resource pool

  /**
   * Trace information regarding Sidelink PSCCH UE scheduling.
   * SlUeMacStatParameters (see lte-common.h)
   */
 TracedCallback<SlUeMacStatParameters> m_slPscchScheduling;  //m_slUeScheduling

 /**
  * Trace information regarding Sidelink PSSCH UE scheduling.
  * SlUeMacStatParameters (see lte-common.h)
  */
 TracedCallback<SlUeMacStatParameters> m_slPsschScheduling; //m_slSharedChUeScheduling

 bool m_slHasDataToTx; ///< True if there is data to transmit in the PSSCH

 bool m_sidelinkEnabled; ///< True if Sidelink is used

  /**
   * The `DiscoveryMsgSent` trace source. Track the transmission of discovery message (announce)
   * Exporting RNTI, ProSe App Code.
   */
 TracedCallback<uint16_t, uint32_t> m_discoveryAnnouncementTrace;
};

} // namespace ns3

#endif // LTE_UE_MAC_ENTITY
