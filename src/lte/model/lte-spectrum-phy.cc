/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009, 2011 CTTC
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
 * Author: Nicola Baldo <nbaldo@cttc.es>
 *         Giuseppe Piro  <g.piro@poliba.it>
 *         Marco Miozzo <marco.miozzo@cttc.es> (add physical error model)
 * Modified by: NIST // Contributions may not be subject to US copyright.
 */


#include <ns3/object-factory.h>
#include <ns3/log.h>
#include <cmath>
#include <ns3/simulator.h>
#include <ns3/trace-source-accessor.h>
#include <ns3/antenna-model.h>
#include "lte-spectrum-phy.h"
#include "lte-spectrum-signal-parameters.h"
#include "lte-net-device.h"
#include "lte-radio-bearer-tag.h"
#include "lte-chunk-processor.h"
#include "lte-sl-chunk-processor.h"
#include "lte-phy-tag.h"
#include <ns3/lte-mi-error-model.h>
#include <ns3/lte-radio-bearer-tag.h>
#include <ns3/boolean.h>
#include <ns3/double.h>
#include <ns3/config.h>
#include <ns3/node.h>
#include "ns3/enum.h"
#include <ns3/pointer.h>
#include <ns3/lte-ue-net-device.h>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LteSpectrumPhy");


/// duration of SRS portion of UL subframe  
/// = 1 symbol for SRS -1ns as margin to avoid overlapping simulator events
static const Time UL_SRS_DURATION = NanoSeconds (71429 -1);  

/// duration of the control portion of a subframe
/// = 0.001 / 14 * 3 (ctrl fixed to 3 symbols) -1ns as margin to avoid overlapping simulator events
static const Time DL_CTRL_DURATION = NanoSeconds (214286 -1);

/// Effective coding rate
static const double EffectiveCodingRate[29] = {
  0.08,
  0.1,
  0.11,
  0.15,
  0.19,
  0.24,
  0.3,
  0.37,
  0.44,
  0.51,
  0.3,
  0.33,
  0.37,
  0.42,
  0.48,
  0.54,
  0.6,
  0.43,
  0.45,
  0.5,
  0.55,
  0.6,
  0.65,
  0.7,
  0.75,
  0.8,
  0.85,
  0.89,
  0.92
};



  
TbId_t::TbId_t ()
{
}

TbId_t::TbId_t (const uint16_t a, const uint8_t b)
: m_rnti (a),
  m_layer (b)
{
}

/**
 * Equality operator
 *
 * \param a lhs
 * \param b rhs
 * \returns true if rnti and layer are equal
 */
bool
operator == (const TbId_t &a, const TbId_t &b)
{
  return ( (a.m_rnti == b.m_rnti) && (a.m_layer == b.m_layer) );
}

/**
 * Less than operator
 *
 * \param a lhs
 * \param b rhs
 * \returns true if rnti less than ro rnti equal and layer less than
 */
bool
operator < (const TbId_t& a, const TbId_t& b)
{
  return ( (a.m_rnti < b.m_rnti) || ( (a.m_rnti == b.m_rnti) && (a.m_layer < b.m_layer) ) );
}

SlTbId_t::SlTbId_t ()
{
}

SlTbId_t::SlTbId_t (const uint16_t a, const uint8_t b)
: m_rnti (a),
  m_l1dst (b)
{
}

/**
 * Equality operator
 *
 * \param a lhs
 * \param b rhs
 * \returns true if the rnti and l1dst of the first
 * parameter are equal to the rnti and l1dst of the
 * second parameter
 */
bool
operator == (const SlTbId_t &a, const SlTbId_t &b)
{
  return ( (a.m_rnti == b.m_rnti) && (a.m_l1dst == b.m_l1dst) );
}

/**
 * Less than operator
 *
 * \param a lhs
 * \param b rhs
 * \returns true if the rnti of the first
 * parameter is less than the rnti of the
 * second parameter. OR returns true if the
 * rntis are equal and l1dst of the first
 * parameter is less than the l1dst of the
 * second parameter
 */
bool
operator < (const SlTbId_t& a, const SlTbId_t& b)
{
  return ( (a.m_rnti < b.m_rnti) || ( (a.m_rnti == b.m_rnti) && (a.m_l1dst < b.m_l1dst) ) );
}

/**
 * Equality operator
 *
 * \param a lhs
 * \param b rhs
 * \returns true if the SINR of the first
 * parameter is equal to the SINR of the
 * second parameter
 */
bool
operator == (const SlCtrlPacketInfo_t &a, const SlCtrlPacketInfo_t &b)
{
  return (a.sinr == b.sinr);
}

/**
 * Less than operator
 * Used as comparison rule for std::multiset
 * of SlCtrlPacketInfo_t objects
 *
 * \param a lhs
 * \param b rhs
 * \returns true if the SINR of the first
 * parameter is greater than SINR of the
 * second parameter OR the index of the first
 * parameter is less than the index of the
 * second parameter
 */
bool
operator < (const SlCtrlPacketInfo_t& a, const SlCtrlPacketInfo_t& b)
{
  //we want by decreasing SINR. The second condition will make
  //sure that the two TBs with equal SINR are inserted in increasing
  //order of the index.
  return (a.sinr > b.sinr) || (a.index < b.index);
}

SlDiscTbId_t::SlDiscTbId_t ()
{
}

SlDiscTbId_t::SlDiscTbId_t (const uint16_t a, const uint8_t b)
: m_rnti (a),
  m_resPsdch (b)
{
}

/**
 * Equality operator
 *
 * \param a lhs
 * \param b rhs
 * \returns true if the rnti and PSDCH resource index of the first
 * parameter are equal to the rnti and PSDCH resource index of the
 * second parameter
 */
bool
operator == (const SlDiscTbId_t &a, const SlDiscTbId_t &b)
{
  return ( (a.m_rnti == b.m_rnti) && (a.m_resPsdch == b.m_resPsdch) );
}

/**
 * Less than operator
 *
 * \param a lhs
 * \param b rhs
 * \returns true if the rnti of the first parameter is less than
 * the rnti of the second parameter. OR returns true if the
 * rntis are equal and PSDCH resource index of the first parameter
 * is less than the PSDCH resource index of the second parameter.
 */
bool
operator < (const SlDiscTbId_t& a, const SlDiscTbId_t& b)
{
  return ( (a.m_rnti < b.m_rnti) || ( (a.m_rnti == b.m_rnti) && (a.m_resPsdch < b.m_resPsdch) ) );
}

NS_OBJECT_ENSURE_REGISTERED (LteSpectrumPhy);

LteSpectrumPhy::LteSpectrumPhy ()
  : m_state (IDLE),
    m_cellId (0),
    m_componentCarrierId (0),
    m_transmissionMode (0),
    m_layersNum (1),
    m_ulDataSlCheck (false),
    m_slssId(0)
{
  NS_LOG_FUNCTION (this);
  m_random = CreateObject<UniformRandomVariable> ();
  m_random->SetAttribute ("Min", DoubleValue (0.0));
  m_random->SetAttribute ("Max", DoubleValue (1.0));
  m_interferenceData = CreateObject<LteInterference> ();
  m_interferenceCtrl = CreateObject<LteInterference> ();
  m_interferenceSl = CreateObject<LteSlInterference> ();

  for (uint8_t i = 0; i < 7; i++)
    {
      m_txModeGain.push_back (1.0);
    }
  
  m_slRxRbStartIdx = 0;
  for (uint32_t i = 0; i < 300; i++)
    {
      m_msgLastReception.push_back(0);
    }
  isTx = false;
  m_nextTxTime = 0;
}


LteSpectrumPhy::~LteSpectrumPhy ()
{
  NS_LOG_FUNCTION (this);
  m_expectedTbs.clear ();
  m_expectedSlTbs.clear ();
  m_txModeGain.clear ();
  m_slDiscTxCount.clear();
}

void LteSpectrumPhy::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  m_channel = 0;
  m_mobility = 0;
  m_device = 0;
  m_interferenceData->Dispose ();
  m_interferenceData = 0;
  m_interferenceCtrl->Dispose ();
  m_interferenceCtrl = 0;
  m_interferenceSl->Dispose ();
  m_interferenceSl = 0;
  m_ulDataSlCheck = false;
  m_ltePhyRxDataEndErrorCallback = MakeNullCallback< void > ();
  m_ltePhyRxDataEndOkCallback    = MakeNullCallback< void, Ptr<Packet> >  ();
  m_ltePhyRxCtrlEndOkCallback = MakeNullCallback< void, std::list<Ptr<LteControlMessage> > > ();
  m_ltePhyRxCtrlEndErrorCallback = MakeNullCallback< void > ();
  m_ltePhyDlHarqFeedbackCallback = MakeNullCallback< void, DlInfoListElement_s > ();
  m_ltePhyUlHarqFeedbackCallback = MakeNullCallback< void, UlInfoListElement_s > ();
  m_ltePhyRxPssCallback = MakeNullCallback< void, uint16_t, Ptr<SpectrumValue> > ();
  m_ltePhyRxSlssCallback = MakeNullCallback< void, uint16_t, Ptr<SpectrumValue> > ();
  SpectrumPhy::DoDispose ();
} 

/**
 * Output stream output operator
 *
 * \param os output stream
 * \param s state
 * \returns output stream
 */
std::ostream& operator<< (std::ostream& os, LteSpectrumPhy::State s)
{
  switch (s)
    {
    case LteSpectrumPhy::IDLE:
      os << "IDLE";
      break;
    case LteSpectrumPhy::RX_DATA:
      os << "RX_DATA";
      break;
    case LteSpectrumPhy::RX_DL_CTRL:
      os << "RX_DL_CTRL";
      break;
    case LteSpectrumPhy::TX_DATA:
      os << "TX_DATA";
      break;
    case LteSpectrumPhy::TX_DL_CTRL:
      os << "TX_DL_CTRL";
      break;
    case LteSpectrumPhy::TX_UL_SRS:
      os << "TX_UL_SRS";
      break;
    default:
      os << "UNKNOWN";
      break;
    }
  return os;
}

TypeId
LteSpectrumPhy::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::LteSpectrumPhy")
    .SetParent<SpectrumPhy> ()
    .SetGroupName("Lte")
    .AddTraceSource ("TxStart",
                     "Trace fired when a new transmission is started",
                     MakeTraceSourceAccessor (&LteSpectrumPhy::m_phyTxStartTrace),
                     "ns3::PacketBurst::TracedCallback")
    .AddTraceSource ("TxEnd",
                     "Trace fired when a previously started transmission is finished",
                     MakeTraceSourceAccessor (&LteSpectrumPhy::m_phyTxEndTrace),
                     "ns3::PacketBurst::TracedCallback")
    .AddTraceSource ("RxStart",
                     "Trace fired when the start of a signal is detected",
                     MakeTraceSourceAccessor (&LteSpectrumPhy::m_phyRxStartTrace),
                     "ns3::PacketBurst::TracedCallback")
    .AddTraceSource ("RxEndOk",
                     "Trace fired when a previously started RX terminates successfully",
                     MakeTraceSourceAccessor (&LteSpectrumPhy::m_phyRxEndOkTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("RxEndError",
                     "Trace fired when a previously started RX terminates with an error",
                     MakeTraceSourceAccessor (&LteSpectrumPhy::m_phyRxEndErrorTrace),
                     "ns3::Packet::TracedCallback")
    .AddAttribute ("DataErrorModelEnabled",
                    "Activate/Deactivate the error model of data (TBs of PDSCH and PUSCH) [by default is active].",
                    BooleanValue (true),
                   MakeBooleanAccessor (&LteSpectrumPhy::m_dataErrorModelEnabled),
                    MakeBooleanChecker ())
    .AddAttribute ("CtrlErrorModelEnabled",
                    "Activate/Deactivate the error model of control (PCFICH-PDCCH decodification) [by default is active].",
                    BooleanValue (true),
                    MakeBooleanAccessor (&LteSpectrumPhy::m_ctrlErrorModelEnabled),
                    MakeBooleanChecker ())
    .AddTraceSource ("DlPhyReception",
                     "DL reception PHY layer statistics.",
                     MakeTraceSourceAccessor (&LteSpectrumPhy::m_dlPhyReception),
                     "ns3::PhyReceptionStatParameters::TracedCallback")
    .AddTraceSource ("UlPhyReception",
                     "DL reception PHY layer statistics.",
                     MakeTraceSourceAccessor (&LteSpectrumPhy::m_ulPhyReception),
                     "ns3::PhyReceptionStatParameters::TracedCallback")
    .AddAttribute ("DropRbOnCollisionEnabled",
                   "Activate/Deactivate the dropping colliding RBs regardless SINR value [by default is not active].",
                   BooleanValue (false),
                   MakeBooleanAccessor (&LteSpectrumPhy::m_dropRbOnCollisionEnabled),
                   MakeBooleanChecker ())
    .AddAttribute ("SlDataErrorModelEnabled",
                   "Activate/Deactivate the error model for the Sidelink PSSCH decodification [by default is active].",
                   BooleanValue (true),
                   MakeBooleanAccessor (&LteSpectrumPhy::m_slDataErrorModelEnabled),
                   MakeBooleanChecker ())
    .AddAttribute ("SlCtrlErrorModelEnabled",
                   "Activate/Deactivate the error model for the Sidelink PSCCH decodification [by default is active].",
                   BooleanValue (true),
                   MakeBooleanAccessor (&LteSpectrumPhy::m_slCtrlErrorModelEnabled),
                   MakeBooleanChecker ())
    .AddAttribute ("SlDiscoveryErrorModelEnabled",
                   "Activate/Deactivate the error model for the Sidelink PSDCH decodification [by default is active].",
                   BooleanValue (true),
                   MakeBooleanAccessor (&LteSpectrumPhy::m_slDiscoveryErrorModelEnabled),
                   MakeBooleanChecker ())
    .AddAttribute ("FadingModel",
                   "Fading model",
                   EnumValue (LteNistErrorModel::AWGN),
                   MakeEnumAccessor (&LteSpectrumPhy::m_fadingModel),
                   MakeEnumChecker (LteNistErrorModel::AWGN, "AWGN"))
    .AddAttribute ("HalfDuplexPhy",
                   "A pointer to a UL LteSpectrumPhy object",
                   PointerValue (),
                   MakePointerAccessor (&LteSpectrumPhy::m_halfDuplexPhy),
                   MakePointerChecker <LteSpectrumPhy> ())
    .AddAttribute ("CtrlFullDuplexEnabled",
                   "Activate/Deactivate the full duplex in the PSCCH [by default is disable].",
                   BooleanValue (false),
                   MakeBooleanAccessor (&LteSpectrumPhy::m_ctrlFullDuplexEnabled),
                   MakeBooleanChecker ())
    .AddTraceSource ("SlPhyReception",
                     "SL reception PHY layer statistics.",
                     MakeTraceSourceAccessor (&LteSpectrumPhy::m_slPhyReception),
                     "ns3::PhyReceptionStatParameters::TracedCallback")
    .AddTraceSource ("SlPscchReception",
                     "SL reception PSCCH PHY layer statistics.",
                     MakeTraceSourceAccessor (&LteSpectrumPhy::m_slPscchReception),
                     "ns3::SlPhyReceptionStatParameters::TracedCallback")
    .AddTraceSource ("SlStartRx",
                     "Trace fired when reception at Sidelink starts.",
                     MakeTraceSourceAccessor (&LteSpectrumPhy::m_slStartRx),
                     "ns3::LteSpectrumPhy::SlStartRxTracedCallback")

  ;
  return tid;
}



Ptr<NetDevice>
LteSpectrumPhy::GetDevice () const
{
  NS_LOG_FUNCTION (this);
  return m_device;
}


Ptr<MobilityModel>
LteSpectrumPhy::GetMobility ()
{
  NS_LOG_FUNCTION (this);
  return m_mobility;
}


void
LteSpectrumPhy::SetDevice (Ptr<NetDevice> d)
{
  NS_LOG_FUNCTION (this << d);
  m_device = d;
}

void
LteSpectrumPhy::SetNodeList (NodeContainer c)
{
  NS_LOG_FUNCTION (this);
  m_nodeList = c;
}

void
LteSpectrumPhy::SetMobility (Ptr<MobilityModel> m)
{
  NS_LOG_FUNCTION (this << m);
  m_mobility = m;
}


void
LteSpectrumPhy::SetChannel (Ptr<SpectrumChannel> c)
{
  NS_LOG_FUNCTION (this << c);
  m_channel = c;
}

void
LteSpectrumPhy::SetRbPerSubChannel (uint32_t rbPerSubChannel)
{
  NS_LOG_FUNCTION (this);
  m_RbPerSubChannel = rbPerSubChannel;
  InitRssiRsrpMap ();
}

void
LteSpectrumPhy::InitRssiRsrpMap ()
{
  NS_LOG_FUNCTION (this);
  uint32_t nSubChannel = std::ceil(50 / m_RbPerSubChannel);

  NS_ASSERT (m_rssiMap.size()==0);
  for (uint32_t subChannel = 0; subChannel < nSubChannel; subChannel++)
    {
      std::vector<double> temp;
      m_rssiMap.push_back(temp);
      for (int subFrame = 0; subFrame < 1000; subFrame++)
        {
          m_rssiMap[subChannel].push_back(0.0);
        }
    }

  NS_ASSERT (m_rsrpMap.size()==0);
  for (uint32_t subChannel = 0; subChannel < nSubChannel; subChannel++)
    {
      std::vector<double> temp;
      m_rsrpMap.push_back(temp);
      for (int subFrame = 0; subFrame < 1000; subFrame++)
        {
          m_rsrpMap[subChannel].push_back(0.0);
        }
    }

  NS_ASSERT (m_decodingMap.size()==0);
  for (uint32_t subChannel = 0; subChannel < nSubChannel; subChannel++)
    {
      std::vector<bool> temp;
      m_decodingMap.push_back(temp);
      for (int subFrame = 0; subFrame < 1000; subFrame++)
        {
          m_decodingMap[subChannel].push_back(false);
        }
    }
  
  /*NS_ASSERT (m_feedbackMap.size()==0);
  for (uint32_t subChannel = 0; subChannel < nSubChannel; subChannel++)
    {
      std::vector<uint32_t> temp;
      m_feedbackMap.push_back(temp);
      for (int subFrame = 0; subFrame < 1000; subFrame++)
        {
          m_feedbackMap[subChannel].push_back(0);
        }
    }*/
}

Ptr<SpectrumChannel> 
LteSpectrumPhy::GetChannel ()
{
  NS_LOG_FUNCTION(this);
  return m_channel;
}

Ptr<const SpectrumModel>
LteSpectrumPhy::GetRxSpectrumModel () const
{
  NS_LOG_FUNCTION(this);
  return m_rxSpectrumModel;
}


void
LteSpectrumPhy::SetTxPowerSpectralDensity (Ptr<SpectrumValue> txPsd)
{
  NS_LOG_FUNCTION (this << txPsd);
  NS_ASSERT (txPsd);
  m_txPsd = txPsd;
}


void
LteSpectrumPhy::SetNoisePowerSpectralDensity (Ptr<const SpectrumValue> noisePsd)
{
  NS_LOG_FUNCTION (this << noisePsd);
  NS_ASSERT (noisePsd);
  m_rxSpectrumModel = noisePsd->GetSpectrumModel ();
  m_interferenceData->SetNoisePowerSpectralDensity (noisePsd);
  m_interferenceCtrl->SetNoisePowerSpectralDensity (noisePsd);
  m_interferenceSl->SetNoisePowerSpectralDensity (noisePsd);
}

  
void 
LteSpectrumPhy::Reset ()
{
  NS_LOG_FUNCTION (this);
  m_cellId = 0;
  m_state = IDLE;
  m_transmissionMode = 0;
  m_layersNum = 1;
  m_endTxEvent.Cancel ();
  m_endRxDataEvent.Cancel ();
  m_endRxDlCtrlEvent.Cancel ();
  m_endRxUlSrsEvent.Cancel ();
  m_rxControlMessageList.clear ();
  m_expectedTbs.clear ();
  m_txControlMessageList.clear ();
  m_rxPacketBurstList.clear ();
  m_txPacketBurst = 0;
  m_rxSpectrumModel = 0;
  m_slssId = 0;
  m_halfDuplexPhy = 0;
  m_ulDataSlCheck = false;
}


void
LteSpectrumPhy::SetLtePhyRxDataEndErrorCallback (LtePhyRxDataEndErrorCallback c)
{
  NS_LOG_FUNCTION (this);
  m_ltePhyRxDataEndErrorCallback = c;
}


void
LteSpectrumPhy::SetLtePhyRxDataEndOkCallback (LtePhyRxDataEndOkCallback c)
{
  NS_LOG_FUNCTION (this);
  m_ltePhyRxDataEndOkCallback = c;
}

void
LteSpectrumPhy::SetLtePhyRxCtrlEndOkCallback (LtePhyRxCtrlEndOkCallback c)
{
  NS_LOG_FUNCTION (this);
  m_ltePhyRxCtrlEndOkCallback = c;
}

void
LteSpectrumPhy::SetLtePhyRxCtrlEndErrorCallback (LtePhyRxCtrlEndErrorCallback c)
{
  NS_LOG_FUNCTION (this);
  m_ltePhyRxCtrlEndErrorCallback = c;
}


void
LteSpectrumPhy::SetLtePhyRxPssCallback (LtePhyRxPssCallback c)
{
  NS_LOG_FUNCTION (this);
  m_ltePhyRxPssCallback = c;
}

void
LteSpectrumPhy::SetLtePhyDlHarqFeedbackCallback (LtePhyDlHarqFeedbackCallback c)
{
  NS_LOG_FUNCTION (this);
  m_ltePhyDlHarqFeedbackCallback = c;
}

void
LteSpectrumPhy::SetLtePhyUlHarqFeedbackCallback (LtePhyUlHarqFeedbackCallback c)
{
  NS_LOG_FUNCTION (this);
  m_ltePhyUlHarqFeedbackCallback = c;
}

void
LteSpectrumPhy::SetLtePhyRxSlssCallback (LtePhyRxSlssCallback c)
{
  NS_LOG_FUNCTION (this);
  m_ltePhyRxSlssCallback = c;
}


Ptr<AntennaModel>
LteSpectrumPhy::GetRxAntenna ()
{
  NS_LOG_FUNCTION(this);
  return m_antenna;
}

void
LteSpectrumPhy::SetAntenna (Ptr<AntennaModel> a)
{
  NS_LOG_FUNCTION (this << a);
  m_antenna = a;
}

void
LteSpectrumPhy::SetState (State newState)
{
  NS_LOG_FUNCTION(this);
  NS_LOG_LOGIC (this << " State: " << m_state << " -> " << newState);
  ChangeState (newState);
}


void
LteSpectrumPhy::ChangeState (State newState)
{
  NS_LOG_FUNCTION(this);
  NS_LOG_LOGIC (this << " State: " << m_state << " -> " << newState);
  m_state = newState;
}


void
LteSpectrumPhy::SetHarqPhyModule (Ptr<LteHarqPhy> harq)
{
  NS_LOG_FUNCTION(this);
  m_harqPhyModule = harq;
}

void
LteSpectrumPhy::SetSlHarqPhyModule (Ptr<LteSlHarqPhy> harq)
{
  NS_LOG_FUNCTION(this);
  m_slHarqPhyModule = harq;
}

void
LteSpectrumPhy::ClearExpectedSlTb ()
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO ("Expected TBs: " << m_expectedSlTbs.size ());
  m_expectedSlTbs.clear ();
  NS_LOG_INFO ("After clearing Expected TBs size: " << m_expectedSlTbs.size ());
}

bool
LteSpectrumPhy::StartTxDataFrame (Ptr<PacketBurst> pb, std::list<Ptr<LteControlMessage> > ctrlMsgList, Time duration)
{
  NS_LOG_FUNCTION (this << pb << " State: " << m_state);
  
  m_phyTxStartTrace (pb);
  
  switch (m_state)
    {
    case RX_DATA:
    case RX_DL_CTRL:
    case RX_UL_SRS:
      NS_FATAL_ERROR ("cannot TX while RX: according to FDD channel access, the physical layer for transmission cannot be used for reception");
      break;

    case TX_DATA:
    case TX_DL_CTRL:      
    case TX_UL_SRS:
      NS_FATAL_ERROR ("cannot TX while already TX: the MAC should avoid this");
      break;
      
    case IDLE:
    {
      /*
      m_txPsd must be set by the device, according to
      (i) the available subchannel for transmission
      (ii) the power transmission
      */
      NS_ASSERT (m_txPsd);
      m_txPacketBurst = pb;
      
      // we need to convey some PHY meta information to the receiver
      // to be used for simulation purposes (e.g., the CellId). This
      // is done by setting the ctrlMsgList parameter of
      // LteSpectrumSignalParametersDataFrame
      ChangeState (TX_DATA);
      NS_ASSERT (m_channel);
      Ptr<LteSpectrumSignalParametersDataFrame> txParams = Create<LteSpectrumSignalParametersDataFrame> ();
      txParams->duration = duration;
      txParams->txPhy = GetObject<SpectrumPhy> ();
      txParams->txAntenna = m_antenna;
      txParams->psd = m_txPsd;
      txParams->packetBurst = pb;
      txParams->ctrlMsgList = ctrlMsgList;
      txParams->cellId = m_cellId;
      if (pb)
        {
          m_ulDataSlCheck = true;
        }
      m_channel->StartTx (txParams);
      m_endTxEvent = Simulator::Schedule (duration, &LteSpectrumPhy::EndTxData, this);
    }
    return false;
    break;
    
    default:
      NS_FATAL_ERROR ("unknown state");
      return true;
      break;
  }
}

bool
LteSpectrumPhy::StartTxSlDataFrame (Ptr<PacketBurst> pb, std::list<Ptr<LteControlMessage> > ctrlMsgList, Time duration, uint8_t groupId)
{
  NS_LOG_INFO (this << pb << " ID:" << GetDevice()->GetNode()->GetId() << " State: " << m_state);

  m_phyTxStartTrace (pb);
  isTx = true;
  switch (m_state)
  {
    //case RX_DATA:
    case RX_DL_CTRL:
    case RX_UL_SRS:
      NS_FATAL_ERROR ("cannot TX while RX: according to FDD channel access, the physical layer for transmission cannot be used for reception");
      break;

    case TX_DATA:
    case TX_DL_CTRL:
    case TX_UL_SRS:
      NS_FATAL_ERROR ("cannot TX while already TX: the MAC should avoid this");
      break;
    case RX_DATA:
      if (!m_ctrlFullDuplexEnabled)
        {
          NS_FATAL_ERROR ("cannot TX while RX: according to FDD channel access, the physical layer for transmission cannot be used for reception");
          break;
        }
    case IDLE:
    {
      /*
      m_txPsd must be set by the device, according to
      (i) the available subchannel for transmission
      (ii) the power transmission
      */
      NS_ASSERT (m_txPsd);
      m_txPacketBurst = pb;

      // we need to convey some PHY meta information to the receiver
      // to be used for simulation purposes (e.g., the CellId). This
      // is done by setting the ctrlMsgList parameter of
      // LteSpectrumSignalParametersDataFrame
      ChangeState (TX_DATA);
      NS_ASSERT (m_channel);
      Ptr<LteSpectrumSignalParametersSlFrame> txParams = Create<LteSpectrumSignalParametersSlFrame> ();
      txParams->duration = duration;
      txParams->txPhy = GetObject<SpectrumPhy> ();
      txParams->txAntenna = m_antenna;
      txParams->psd = m_txPsd;
      txParams->nodeId = GetDevice()->GetNode()->GetId();
      txParams->groupId = groupId;
      txParams->slssId = m_slssId;
      txParams->packetBurst = pb;
      txParams->ctrlMsgList = ctrlMsgList;
      m_ulDataSlCheck = true;

      //NS_LOG_DEBUG("StartTx on Spectrum Channel");
      m_channel->StartTx (txParams);
      m_endTxEvent = Simulator::Schedule (duration, &LteSpectrumPhy::EndTxData, this);
    }
    return false;
    break;

    default:
      NS_FATAL_ERROR ("unknown state");
      return true;
      break;
  }
}

bool
LteSpectrumPhy::StartTxDlCtrlFrame (std::list<Ptr<LteControlMessage> > ctrlMsgList, bool pss)
{
  NS_LOG_FUNCTION (this << " PSS " << pss << " State: " << m_state);
  
  switch (m_state)
  {
    case RX_DATA:
    case RX_DL_CTRL:
    case RX_UL_SRS:
      NS_FATAL_ERROR ("cannot TX while RX: according to FDD channel access, the physical layer for transmission cannot be used for reception");
      break;
      
    case TX_DATA:
    case TX_DL_CTRL:
    case TX_UL_SRS:
      NS_FATAL_ERROR ("cannot TX while already TX: the MAC should avoid this");
      break;
      
    case IDLE:
    {
      /*
      m_txPsd must be set by the device, according to
      (i) the available subchannel for transmission
      (ii) the power transmission
      */
      NS_ASSERT (m_txPsd);
      
      // we need to convey some PHY meta information to the receiver
      // to be used for simulation purposes (e.g., the CellId). This
      // is done by setting the cellId parameter of
      // LteSpectrumSignalParametersDlCtrlFrame
      ChangeState (TX_DL_CTRL);
      NS_ASSERT (m_channel);

      Ptr<LteSpectrumSignalParametersDlCtrlFrame> txParams = Create<LteSpectrumSignalParametersDlCtrlFrame> ();
      txParams->duration = DL_CTRL_DURATION;
      txParams->txPhy = GetObject<SpectrumPhy> ();
      txParams->txAntenna = m_antenna;
      txParams->psd = m_txPsd;
      txParams->cellId = m_cellId;
      txParams->pss = pss;
      txParams->ctrlMsgList = ctrlMsgList;
      m_channel->StartTx (txParams);
      m_endTxEvent = Simulator::Schedule (DL_CTRL_DURATION, &LteSpectrumPhy::EndTxDlCtrl, this);
    }
    return false;
    break;
    
    default:
      NS_FATAL_ERROR ("unknown state");
      return true;
      break;
  }
}


bool
LteSpectrumPhy::StartTxUlSrsFrame ()
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC (this << " State: " << m_state);
  
  switch (m_state)
    {
    case RX_DATA:
    case RX_DL_CTRL:
    case RX_UL_SRS:
      NS_FATAL_ERROR ("cannot TX while RX: according to FDD channel access, the physical layer for transmission cannot be used for reception");
      break;
      
    case TX_DL_CTRL:
    case TX_DATA:
    case TX_UL_SRS:
      NS_FATAL_ERROR ("cannot TX while already TX: the MAC should avoid this");
      break;
      
    case IDLE:
    {
      /*
      m_txPsd must be set by the device, according to
      (i) the available subchannel for transmission
      (ii) the power transmission
      */
      NS_ASSERT (m_txPsd);
      NS_LOG_LOGIC (this << " m_txPsd: " << *m_txPsd);
      
      // we need to convey some PHY meta information to the receiver
      // to be used for simulation purposes (e.g., the CellId). This
      // is done by setting the cellId parameter of 
      // LteSpectrumSignalParametersDlCtrlFrame
      ChangeState (TX_UL_SRS);
      NS_ASSERT (m_channel);
      Ptr<LteSpectrumSignalParametersUlSrsFrame> txParams = Create<LteSpectrumSignalParametersUlSrsFrame> ();
      txParams->duration = UL_SRS_DURATION;
      txParams->txPhy = GetObject<SpectrumPhy> ();
      txParams->txAntenna = m_antenna;
      txParams->psd = m_txPsd;
      txParams->cellId = m_cellId;
      m_channel->StartTx (txParams);
      m_endTxEvent = Simulator::Schedule (UL_SRS_DURATION, &LteSpectrumPhy::EndTxUlSrs, this);
    }
    return false;
    break;
    
    default:
      NS_FATAL_ERROR ("unknown state");
      return true;
      break;
  }
}



void
LteSpectrumPhy::EndTxData ()
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC (this << " State: " << m_state);

  //NS_ASSERT (m_state == TX_DATA);
  m_phyTxEndTrace (m_txPacketBurst);
  m_txPacketBurst = 0;
  ChangeState (IDLE);
  isTx = false;
}

void
LteSpectrumPhy::EndTxDlCtrl ()
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC (this << " State: " << m_state);

  NS_ASSERT (m_state == TX_DL_CTRL);
  NS_ASSERT (m_txPacketBurst == 0);
  ChangeState (IDLE);
}

void
LteSpectrumPhy::EndTxUlSrs ()
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC (this << " State: " << m_state);

  NS_ASSERT (m_state == TX_UL_SRS);
  NS_ASSERT (m_txPacketBurst == 0);
  ChangeState (IDLE);
}




void
LteSpectrumPhy::StartRx (Ptr<SpectrumSignalParameters> spectrumRxParams)
{
  NS_LOG_INFO (this << spectrumRxParams << " State: " << m_state);

  Ptr <const SpectrumValue> rxPsd = spectrumRxParams->psd;
  Time duration = spectrumRxParams->duration;
  
  // the device might start RX only if the signal is of a type
  // understood by this device - in this case, an LTE signal.
  Ptr<LteSpectrumSignalParametersDataFrame> lteDataRxParams = DynamicCast<LteSpectrumSignalParametersDataFrame> (spectrumRxParams);
  Ptr<LteSpectrumSignalParametersDlCtrlFrame> lteDlCtrlRxParams = DynamicCast<LteSpectrumSignalParametersDlCtrlFrame> (spectrumRxParams);
  Ptr<LteSpectrumSignalParametersUlSrsFrame> lteUlSrsRxParams = DynamicCast<LteSpectrumSignalParametersUlSrsFrame> (spectrumRxParams);
  Ptr<LteSpectrumSignalParametersSlFrame> lteSlRxParams = DynamicCast<LteSpectrumSignalParametersSlFrame> (spectrumRxParams);

  if (lteDataRxParams != 0)
    {
      m_interferenceData->AddSignal (rxPsd, duration);
      StartRxData (lteDataRxParams);
    }
  else if (lteDlCtrlRxParams != 0)
    {
      m_interferenceCtrl->AddSignal (rxPsd, duration);
      StartRxDlCtrl (lteDlCtrlRxParams);
    }
  else if (lteUlSrsRxParams != 0)
    {
      m_interferenceCtrl->AddSignal (rxPsd, duration);
      StartRxUlSrs (lteUlSrsRxParams);
    }
  else if (lteSlRxParams != 0)
    {
      m_interferenceSl->AddSignal (rxPsd, duration);
      m_interferenceData->AddSignal (rxPsd, duration); //to compute UL/SL interference
      m_slStartRx (m_halfDuplexPhy);
      if (m_ctrlFullDuplexEnabled && lteSlRxParams->ctrlMsgList.size () > 0)
        {
          StartRxSlData (lteSlRxParams);
        }
      else if (m_halfDuplexPhy != 0)
        {
          if (m_halfDuplexPhy->GetState () == IDLE || !(m_halfDuplexPhy->m_ulDataSlCheck))
            {
              NS_LOG_INFO (this<<" Received Sidelink Data "<<m_halfDuplexPhy);
              StartRxSlData (lteSlRxParams);
            }
        }
    }
  else
    {
      // other type of signal (could be 3G, GSM, whatever) -> interference
      m_interferenceData->AddSignal (rxPsd, duration);
      m_interferenceCtrl->AddSignal (rxPsd, duration);
      m_interferenceSl->AddSignal (rxPsd, duration);
    }    
}

void
LteSpectrumPhy::StartRxData (Ptr<LteSpectrumSignalParametersDataFrame> params)
{
  NS_LOG_FUNCTION (this);
  switch (m_state)
    {
      case TX_DATA:
      case TX_DL_CTRL:
      case TX_UL_SRS:
        NS_FATAL_ERROR ("cannot RX while TX: according to FDD channel access, the physical layer for transmission cannot be used for reception");
        break;
      case RX_DL_CTRL:
        NS_FATAL_ERROR ("cannot RX Data while receiving control");
        break;
      case IDLE:
      case RX_DATA:
        // the behavior is similar when
        // we're IDLE or RX because we can receive more signals
        // simultaneously (e.g., at the eNB).
        {
          // To check if we're synchronized to this signal, we check
          // for the CellId which is reported in the
          //  LteSpectrumSignalParametersDataFrame
          if (params->cellId  == m_cellId)
            {
              NS_LOG_LOGIC (this << " synchronized with this signal (cellId=" << params->cellId << ")");
              if ((m_rxPacketBurstList.empty ())&&(m_rxControlMessageList.empty ()))
                {
                  NS_ASSERT (m_state == IDLE);
                  // first transmission, i.e., we're IDLE and we
                  // start RX
                  m_firstRxStart = Simulator::Now ();
                  m_firstRxDuration = params->duration;
                  NS_LOG_LOGIC (this << " scheduling EndRx with delay " << params->duration.GetSeconds () << "s");
                  m_endRxDataEvent = Simulator::Schedule (params->duration, &LteSpectrumPhy::EndRxData, this);
                }
              else
                {
                  NS_ASSERT (m_state == RX_DATA);
                  // sanity check: if there are multiple RX events, they
                  // should occur at the same time and have the same
                  // duration, otherwise the interference calculation
                  // won't be correct
                  NS_ASSERT ((m_firstRxStart == Simulator::Now ()) 
                  && (m_firstRxDuration == params->duration));
                }
              
              ChangeState (RX_DATA);
              if (params->packetBurst)
                {
                  m_rxPacketBurstList.push_back (params->packetBurst);
                  m_interferenceData->StartRx (params->psd);
                  
                  m_phyRxStartTrace (params->packetBurst);
                }
                NS_LOG_DEBUG (this << " insert msgs " << params->ctrlMsgList.size ());
              m_rxControlMessageList.insert (m_rxControlMessageList.end (), params->ctrlMsgList.begin (), params->ctrlMsgList.end ());
              
              NS_LOG_LOGIC (this << " numSimultaneousRxEvents = " << m_rxPacketBurstList.size ());
            }
          else
            {
              NS_LOG_LOGIC (this << " not in sync with this signal (cellId=" 
              << params->cellId  << ", m_cellId=" << m_cellId << ")");
            }
        }
        break;
        
        default:
          NS_FATAL_ERROR ("unknown state");
          break;
      }
      
   NS_LOG_LOGIC (this << " State: " << m_state);
}


void
LteSpectrumPhy::StartRxSlData (Ptr<LteSpectrumSignalParametersSlFrame> params)
{
  NS_LOG_INFO (this <<" Cell ID: "<<m_cellId<<" Node ID: " << GetDevice()->GetNode()->GetId() << " State: " << m_state);

  switch (m_state)
    {
      //case TX_DATA:
      case TX_DL_CTRL:
      case TX_UL_SRS:
      NS_FATAL_ERROR ("cannot RX while TX: according to FDD channel access, the physical layer for transmission cannot be used for reception");
      break;
      case RX_DL_CTRL:
      NS_FATAL_ERROR ("cannot RX Data while receiving control");
      break;
      case TX_DATA:
        if (!m_ctrlFullDuplexEnabled)
          {
            break;
          }
      case IDLE:
      case RX_DATA:
        // the behavior is similar when
        // we're IDLE or RX because we can receive more signals
        // simultaneously (e.g., at the eNB).
        {
          // check it is not an eNB and not the same sending node (Sidelink : discovery & communication )
          if(m_cellId == 0 && params->nodeId != GetDevice()->GetNode()->GetId())
            {
              NS_LOG_LOGIC ("the signal is neither from eNodeB nor from this UE");
              NS_LOG_INFO ("Signal is from Node id = "<<params->nodeId);

              //SLSSs (PSBCH) should be received by all UEs
              //Checking if it is a SLSS, and if it is: measure S-RSRP and receive MIB-SL
              if (params->ctrlMsgList.size () >0)
                {
                  std::list<Ptr<LteControlMessage> >::iterator ctrlIt;
                  for (ctrlIt=params->ctrlMsgList.begin() ; ctrlIt != params->ctrlMsgList.end(); ctrlIt++)
                    {
                      //Detection of a SLSS and callback for measurement of S-RSRP
                      if ((*ctrlIt)->GetMessageType () == LteControlMessage::MIB_SL)
                        {
                          NS_LOG_LOGIC ("Receiving a SLSS");
                          Ptr<MibSlLteControlMessage> msg = DynamicCast<MibSlLteControlMessage> (*ctrlIt);
                          LteRrcSap::MasterInformationBlockSL mibSL = msg->GetMibSL ();
                          //Measure S-RSRP
                          if (!m_ltePhyRxSlssCallback.IsNull ())
                            {
                              m_ltePhyRxSlssCallback (mibSL.slssid, params->psd); //LteUePhy::ReceiveSlss
                            }
                          //Receive MIB-SL
                          if (m_rxPacketInfo.empty ())
                            {
                              NS_ASSERT (m_state == IDLE);
                              // first transmission, i.e., we're IDLE and we start RX
                              m_firstRxStart = Simulator::Now ();
                              m_firstRxDuration = params->duration;
                              NS_LOG_LOGIC (this << " scheduling EndRxSl with delay " << params->duration.GetSeconds () << "s");

                              m_endRxDataEvent = Simulator::Schedule (params->duration, &LteSpectrumPhy::EndRxSlData, this);
                            }
                          else
                            {
                              NS_ASSERT (m_state == RX_DATA);
                              // sanity check: if there are multiple RX events, they
                              // should occur at the same time and have the same
                              // duration, otherwise the interference calculation
                              // won't be correct
                              NS_ASSERT ((m_firstRxStart == Simulator::Now ())
                                         && (m_firstRxDuration == params->duration));
                            }
                          ChangeState (RX_DATA);
                          m_interferenceSl->StartRx (params->psd);
                          SlRxPacketInfo_t packetInfo;
                          packetInfo.m_rxPacketBurst = params->packetBurst;
                          packetInfo.m_rxControlMessage = *ctrlIt;
                          //convert the PSD to RB map so we know which RBs were used to transmit the control message
                          //will be used later to compute error rate
                          std::vector <int> rbMap;
                          int i = 0;
                          for (Values::const_iterator it=params->psd->ConstValuesBegin (); it != params->psd->ConstValuesEnd () ; it++, i++)
                            {
                              if (*it != 0)
                                {
                                  NS_LOG_INFO ("SL MIB-SL arriving on RB " << i);
                                  rbMap.push_back (i);
                                }
                            }
                          packetInfo.rbBitmap = rbMap;
                          m_rxPacketInfo.push_back (packetInfo);
                          params->ctrlMsgList.erase(ctrlIt);
                          break;
                        }
                    }
                }

              //Receive PSCCH, PSSCH and PSDCH only if synchronized to the transmitter (having the same SLSSID)
              //and belonging to the destination group

              if (params->slssId == m_slssId && (params->groupId == 0 || m_l1GroupIds.find (params->groupId) != m_l1GroupIds.end()))
                {
                  NS_LOG_INFO ("Synchronized to transmitter. Already ready to receive PSCCH, PSSCH");
                  if (m_rxPacketInfo.empty ())
                    {
                      //NS_ASSERT (m_state == IDLE);
                      // first transmission, i.e., we're IDLE and we start RX
                      m_firstRxStart = Simulator::Now ();
                      m_firstRxDuration = params->duration;
                      NS_LOG_LOGIC ("Scheduling EndRxSl with delay " << params->duration.GetSeconds () << "s");
                      m_endRxDataEvent = Simulator::Schedule (params->duration, &LteSpectrumPhy::EndRxSlData, this);
                    }
                  else
                    {
                      //NS_ASSERT (m_state == RX_DATA);
                      // sanity check: if there are multiple RX events, they
                      // should occur at the same time and have the same
                      // duration, otherwise the interference calculation
                      // won't be correct
                      NS_ASSERT ((m_firstRxStart == Simulator::Now ())
                                  && (m_firstRxDuration == params->duration));
                    }
                  ChangeState (RX_DATA);
                  m_interferenceSl->StartRx (params->psd);
                  SlRxPacketInfo_t packetInfo;
                  packetInfo.m_rxPacketBurst = params->packetBurst;
                  if (params->ctrlMsgList.size () >0)
                    {
                      NS_ASSERT (params->ctrlMsgList.size () == 1);
                      packetInfo.m_rxControlMessage = *(params->ctrlMsgList.begin());
                    }
                  //convert the PSD to RB map so we know which RBs were used to transmit the control message
                  //will be used later to compute error rate
                  std::vector <int> rbMap;
                  int i = 0;
                  int used_rb_cnt = 0;
                  for (Values::const_iterator it=params->psd->ConstValuesBegin (); it != params->psd->ConstValuesEnd () ; it++, i++)
                    {
                      if (*it != 0)
                        {
                          if (used_rb_cnt == 0)
                            {
                              m_slRxRbStartIdx = i;
                            }
                          NS_LOG_INFO ("SL Message arriving on RB " << i);
                          rbMap.push_back (i);
                          used_rb_cnt++;
                        }
                    }
                  NS_LOG_INFO ("SL Message arriving on "<<used_rb_cnt<<" RBs");
                  packetInfo.rbBitmap = rbMap;
                  m_rxPacketInfo.push_back (packetInfo);
                  if (params->packetBurst)
                    {
                      m_phyRxStartTrace (params->packetBurst);
                      NS_LOG_INFO ("RX Burst containing " << params->packetBurst->GetNPackets() << " packets");
                    }
                  NS_LOG_INFO ("Insert Sidelink ctrl msgs " << params->ctrlMsgList.size ());
                  NS_LOG_LOGIC ("numSimultaneousRxEvents = " << m_rxPacketInfo.size ());
                }
              else
                {
                  NS_LOG_DEBUG ("Not in sync with this Sidelink signal... Ignoring ");
                }
            }
          else
            {
              NS_LOG_LOGIC (this << " the signal is from eNodeB or from this UE... Ignoring. Cell id "<<m_cellId);
              NS_LOG_DEBUG (this << " Node Id from signal " <<params->nodeId<<" My node ID = "<<GetDevice()->GetNode()->GetId());
            }
        }
        break;

      default:
        NS_FATAL_ERROR ("unknown state");
        break;
  }
  NS_LOG_LOGIC (" Exiting StartRxSlData. State: " << m_state);
}



void
LteSpectrumPhy::StartRxDlCtrl (Ptr<LteSpectrumSignalParametersDlCtrlFrame> lteDlCtrlRxParams)
{
  NS_LOG_FUNCTION (this);

  // To check if we're synchronized to this signal, we check
  // for the CellId which is reported in the
  // LteSpectrumSignalParametersDlCtrlFrame
  uint16_t cellId;        
  NS_ASSERT (lteDlCtrlRxParams != 0);
  cellId = lteDlCtrlRxParams->cellId;

  switch (m_state)
    {
    case TX_DATA:
    case TX_DL_CTRL:
    case TX_UL_SRS:
    case RX_DATA:
    case RX_UL_SRS:
      NS_FATAL_ERROR ("unexpected event in state " << m_state);
      break;

    case RX_DL_CTRL:
    case IDLE:

      // common code for the two states
      // check presence of PSS for UE measurements
      if (lteDlCtrlRxParams->pss == true)
        {
          if (!m_ltePhyRxPssCallback.IsNull ())
              {
                m_ltePhyRxPssCallback (cellId, lteDlCtrlRxParams->psd);
              }
        }   

      // differentiated code for the two states
      switch (m_state)
        {
        case RX_DL_CTRL:
          NS_ASSERT_MSG (m_cellId != cellId, "any other DlCtrl should be from a different cell");
          NS_LOG_LOGIC (this << " ignoring other DlCtrl (cellId=" 
                        << cellId  << ", m_cellId=" << m_cellId << ")");      
          break;
          
        case IDLE:
          if (cellId  == m_cellId)
            {
              NS_LOG_LOGIC (this << " synchronized with this signal (cellId=" << cellId << ")");
              
              NS_ASSERT (m_rxControlMessageList.empty ());
              m_firstRxStart = Simulator::Now ();
              m_firstRxDuration = lteDlCtrlRxParams->duration;
              NS_LOG_LOGIC (this << " scheduling EndRx with delay " << lteDlCtrlRxParams->duration);
              
              // store the DCIs
              m_rxControlMessageList = lteDlCtrlRxParams->ctrlMsgList;
              m_endRxDlCtrlEvent = Simulator::Schedule (lteDlCtrlRxParams->duration, &LteSpectrumPhy::EndRxDlCtrl, this);
              ChangeState (RX_DL_CTRL);
              m_interferenceCtrl->StartRx (lteDlCtrlRxParams->psd);            
            }
          else
            {
              NS_LOG_LOGIC (this << " not synchronizing with this signal (cellId=" 
                            << cellId  << ", m_cellId=" << m_cellId << ")");          
            }
          break;
          
        default:
          NS_FATAL_ERROR ("unexpected event in state " << m_state);
          break;
        }
      break; // case RX_DL_CTRL or IDLE
      
    default:
      NS_FATAL_ERROR ("unknown state");
      break;
    }
  
  NS_LOG_LOGIC (this << " State: " << m_state);
}




void
LteSpectrumPhy::StartRxUlSrs (Ptr<LteSpectrumSignalParametersUlSrsFrame> lteUlSrsRxParams)
{
  NS_LOG_FUNCTION (this);
  switch (m_state)
  {
    case TX_DATA:
    case TX_DL_CTRL:
    case TX_UL_SRS:
      NS_FATAL_ERROR ("cannot RX while TX: according to FDD channel access, the physical layer for transmission cannot be used for reception");
      break;

    case RX_DATA:
    case RX_DL_CTRL:
      NS_FATAL_ERROR ("cannot RX SRS while receiving something else");
      break;

    case IDLE:
    case RX_UL_SRS:
      // the behavior is similar when
      // we're IDLE or RX_UL_SRS because we can receive more signals
      // simultaneously at the eNB
      {
        // To check if we're synchronized to this signal, we check
        // for the CellId which is reported in the
        // LteSpectrumSignalParametersDlCtrlFrame
        uint16_t cellId;
        cellId = lteUlSrsRxParams->cellId;
        if (cellId  == m_cellId)
          {
            NS_LOG_LOGIC (this << " synchronized with this signal (cellId=" << cellId << ")");
            if (m_state == IDLE)
              {
                // first transmission, i.e., we're IDLE and we
                // start RX
                NS_ASSERT (m_rxControlMessageList.empty ());
                m_firstRxStart = Simulator::Now ();
                m_firstRxDuration = lteUlSrsRxParams->duration;
                NS_LOG_LOGIC (this << " scheduling EndRx with delay " << lteUlSrsRxParams->duration);

                m_endRxUlSrsEvent = Simulator::Schedule (lteUlSrsRxParams->duration, &LteSpectrumPhy::EndRxUlSrs, this);
              }
            else if (m_state == RX_UL_SRS)
              {
                // sanity check: if there are multiple RX events, they
                // should occur at the same time and have the same
                // duration, otherwise the interference calculation
                // won't be correct
                NS_ASSERT ((m_firstRxStart == Simulator::Now ())
                           && (m_firstRxDuration == lteUlSrsRxParams->duration));
              }
            ChangeState (RX_UL_SRS);
            m_interferenceCtrl->StartRx (lteUlSrsRxParams->psd);
          }
        else
          {
            NS_LOG_LOGIC (this << " not in sync with this signal (cellId="
                          << cellId  << ", m_cellId=" << m_cellId << ")");
          }
      }
      break;

    default:
      NS_FATAL_ERROR ("unknown state");
      break;
  }
  
  NS_LOG_LOGIC (this << " State: " << m_state);
}


void
LteSpectrumPhy::UpdateSinrPerceived (const SpectrumValue& sinr)
{
  NS_LOG_FUNCTION (this << sinr);
  m_sinrPerceived = sinr;
}

void
LteSpectrumPhy::UpdateSlSinrPerceived (std::vector <SpectrumValue> sinr)
{
  NS_LOG_FUNCTION (this);
  m_slSinrPerceived = sinr;
}

void
LteSpectrumPhy::UpdateSlSigPerceived (std::vector <SpectrumValue> signal)
{
  NS_LOG_FUNCTION (this);
  m_slSignalPerceived = signal;
}

void
LteSpectrumPhy::UpdateSlIntPerceived (std::vector <SpectrumValue> interference)
{
  NS_LOG_FUNCTION (this);
  m_slInterferencePerceived = interference;
}

void
LteSpectrumPhy::AddExpectedTb (uint16_t  rnti, uint8_t ndi, uint16_t size, uint8_t mcs, std::vector<int> map, uint8_t layer, uint8_t harqId,uint8_t rv,  bool downlink)
{
  NS_LOG_FUNCTION (this << " RNTI: " << rnti << " NDI " << (uint16_t)ndi << " Size " << size << " MCS " << (uint16_t)mcs << " Layer " << (uint16_t)layer << " Rv " << (uint16_t)rv);
  TbId_t tbId;
  tbId.m_rnti = rnti;
  tbId.m_layer = layer;
  expectedTbs_t::iterator it;
  it = m_expectedTbs.find (tbId);
  if (it != m_expectedTbs.end ())
    {
      // might be a TB of an unreceived packet (due to high path loss)
      m_expectedTbs.erase (it);
    }
  // insert new entry
  tbInfo_t tbInfo = {ndi, size, mcs, map, harqId, rv, 0.0, downlink, false, false};
  m_expectedTbs.insert (std::pair<TbId_t, tbInfo_t> (tbId,tbInfo));
}

void
LteSpectrumPhy::AddExpectedTb (uint16_t  rnti, uint8_t l1dst, uint8_t ndi, uint16_t size, uint8_t mcs, std::vector<int> map, uint8_t rv)
{
  NS_LOG_FUNCTION (this <<" RNTI: " << rnti << " Group " << (uint16_t) l1dst << " NDI " << (uint16_t)ndi << " Size " << size << " MCS " << (uint16_t)mcs << " RV " << (uint16_t)rv);
  SlTbId_t tbId;
  tbId.m_rnti = rnti;
  tbId.m_l1dst = l1dst;
  expectedSlTbs_t::iterator it;
  it = m_expectedSlTbs.find (tbId);
  if (it != m_expectedSlTbs.end ())
    {
      // might be a TB of an unreceived packet (due to high path loss)
      m_expectedSlTbs.erase (it);
    }
  // insert new entry
  SltbInfo_t tbInfo = {ndi, size, mcs, map, rv, 0.0, false, false};
  m_expectedSlTbs.insert (std::pair<SlTbId_t, SltbInfo_t> (tbId,tbInfo));

  // if it is for new data, reset the HARQ process
  if (ndi)
    {
      m_slHarqPhyModule->ResetSlHarqProcessStatus (rnti, l1dst);
      m_slHarqPhyModule->ResetPrevDecoded (rnti, l1dst);
      m_slHarqPhyModule->ResetTbIdx (rnti, l1dst);
    }
}

void
LteSpectrumPhy::AddExpectedTb (uint16_t  rnti, uint8_t resPsdch, uint8_t ndi, std::vector<int> map, uint8_t rv)
{
  NS_LOG_FUNCTION (this << " RNTI: " << rnti << " resPsdch " << resPsdch << " NDI " << (uint16_t)ndi << " RV " << (uint16_t)rv);

  SlDiscTbId_t tbId;
  tbId.m_rnti = rnti;
  tbId.m_resPsdch = resPsdch;
  expectedDiscTbs_t::iterator it;
  it = m_expectedDiscTbs.find (tbId);
  if (it != m_expectedDiscTbs.end ())
    {
      //might be a TB of an unreceived packet (due to high path loss)
      m_expectedDiscTbs.erase (it);
    }
  // insert new entry
  SlDisctbInfo_t tbInfo = {ndi, resPsdch, map, rv, 0.0, false, false};

  m_expectedDiscTbs.insert (std::pair<SlDiscTbId_t, SlDisctbInfo_t> (tbId,tbInfo));

  // if it is for new data, reset the HARQ process
  if (ndi)
    {
      m_slHarqPhyModule->ResetDiscHarqProcessStatus (rnti, resPsdch);
      m_slHarqPhyModule->ResetDiscTbPrevDecoded (rnti, resPsdch);
    }
}


void
LteSpectrumPhy::EndRxData ()
{
  NS_LOG_FUNCTION (this << " State: " << m_state);

  NS_ASSERT (m_state == RX_DATA);

  // this will trigger CQI calculation and Error Model evaluation
  // as a side effect, the error model should update the error status of all TBs
  m_interferenceData->EndRx ();
  NS_LOG_DEBUG (this << " No. of burts " << m_rxPacketBurstList.size ());
  NS_LOG_DEBUG (this << " Expected TBs " << m_expectedTbs.size ());
  expectedTbs_t::iterator itTb = m_expectedTbs.begin ();
  
  // apply transmission mode gain
  NS_LOG_DEBUG (this << " txMode " << (uint16_t)m_transmissionMode << " gain " << m_txModeGain.at (m_transmissionMode));
  NS_ASSERT (m_transmissionMode < m_txModeGain.size ());
  m_sinrPerceived *= m_txModeGain.at (m_transmissionMode);
  
  while (itTb!=m_expectedTbs.end ())
    {
      if ((m_dataErrorModelEnabled)&&(m_rxPacketBurstList.size ()>0)) // avoid to check for errors when there is no actual data transmitted
        {
          // retrieve HARQ info
          HarqProcessInfoList_t harqInfoList;
          if ((*itTb).second.ndi == 0)
            {
              // TB retxed: retrieve HARQ history
              uint16_t ulHarqId = 0;
              if ((*itTb).second.downlink)
                {
                  harqInfoList = m_harqPhyModule->GetHarqProcessInfoDl ((*itTb).second.harqProcessId, (*itTb).first.m_layer);
                }
              else
                {
                  harqInfoList = m_harqPhyModule->GetHarqProcessInfoUl ((*itTb).first.m_rnti, ulHarqId);
                }
            }
          TbStats_t tbStats = LteMiErrorModel::GetTbDecodificationStats (m_sinrPerceived, (*itTb).second.rbBitmap, (*itTb).second.size, (*itTb).second.mcs, harqInfoList);
          (*itTb).second.mi = tbStats.mi;
          (*itTb).second.corrupt = m_random->GetValue () > tbStats.tbler ? false : true;
          NS_LOG_DEBUG (this << "RNTI " << (*itTb).first.m_rnti << " size " << (*itTb).second.size << " mcs " << (uint32_t)(*itTb).second.mcs << " bitmap " << (*itTb).second.rbBitmap.size () << " layer " << (uint16_t)(*itTb).first.m_layer << " TBLER " << tbStats.tbler << " corrupted " << (*itTb).second.corrupt);
          // fire traces on DL/UL reception PHY stats
          PhyReceptionStatParameters params;
          params.m_timestamp = Simulator::Now ().GetMilliSeconds ();
          params.m_cellId = m_cellId;
          params.m_imsi = 0; // it will be set by DlPhyTransmissionCallback in LteHelper
          params.m_rnti = (*itTb).first.m_rnti;
          params.m_txMode = m_transmissionMode;
          params.m_layer =  (*itTb).first.m_layer;
          params.m_mcs = (*itTb).second.mcs;
          params.m_size = (*itTb).second.size;
          params.m_rv = (*itTb).second.rv;
          params.m_ndi = (*itTb).second.ndi;
          params.m_correctness = (uint8_t)!(*itTb).second.corrupt;
          params.m_ccId = m_componentCarrierId;
          SpectrumValue sinrCopy = m_sinrPerceived;
          std::vector<int> map = (*itTb).second.rbBitmap;
          double sum = 0.0;
          for (uint32_t i = 0; i < map.size (); ++i)
            {
              double sinrLin = sinrCopy[map.at (i)];
              sum = sum + sinrLin;
            }
          params.m_sinrPerRb = sum / map.size ();

          if ((*itTb).second.downlink)
            {
              // DL
              m_dlPhyReception (params);
            }
          else
            {
              // UL
              params.m_rv = harqInfoList.size ();
              m_ulPhyReception (params);
            }
       }
      
      itTb++;
    }
    std::map <uint16_t, DlInfoListElement_s> harqDlInfoMap;
    for (std::list<Ptr<PacketBurst> >::const_iterator i = m_rxPacketBurstList.begin (); 
    i != m_rxPacketBurstList.end (); ++i)
      {
        for (std::list<Ptr<Packet> >::const_iterator j = (*i)->Begin (); j != (*i)->End (); ++j)
          {
            // retrieve TB info of this packet 
            LteRadioBearerTag tag;
            (*j)->PeekPacketTag (tag);
            TbId_t tbId;
            tbId.m_rnti = tag.GetRnti ();
            tbId.m_layer = tag.GetLayer ();
            itTb = m_expectedTbs.find (tbId);
            NS_LOG_INFO (this << " Packet of " << tbId.m_rnti << " layer " <<  (uint16_t) tag.GetLayer ());
            if (itTb!=m_expectedTbs.end ())
              {
                if (!(*itTb).second.corrupt)
                  {
                    m_phyRxEndOkTrace (*j);
                
                    if (!m_ltePhyRxDataEndOkCallback.IsNull ())
                      {
                        m_ltePhyRxDataEndOkCallback (*j);
                      }
                  }
                else
                  {
                    // TB received with errors
                    m_phyRxEndErrorTrace (*j);
                  }

                // send HARQ feedback (if not already done for this TB)
                if (!(*itTb).second.harqFeedbackSent)
                  {
                    (*itTb).second.harqFeedbackSent = true;
                    if (!(*itTb).second.downlink)
                      {
                        UlInfoListElement_s harqUlInfo;
                        harqUlInfo.m_rnti = tbId.m_rnti;
                        harqUlInfo.m_tpc = 0;
                        if ((*itTb).second.corrupt)
                          {
                            harqUlInfo.m_receptionStatus = UlInfoListElement_s::NotOk;
                            NS_LOG_DEBUG (this << " RNTI " << tbId.m_rnti << " send UL-HARQ-NACK");
                            m_harqPhyModule->UpdateUlHarqProcessStatus (tbId.m_rnti, (*itTb).second.mi, (*itTb).second.size, (*itTb).second.size / EffectiveCodingRate [(*itTb).second.mcs]);
                          }
                        else
                          {
                            harqUlInfo.m_receptionStatus = UlInfoListElement_s::Ok;
                            NS_LOG_DEBUG (this << " RNTI " << tbId.m_rnti << " send UL-HARQ-ACK");
                            m_harqPhyModule->ResetUlHarqProcessStatus (tbId.m_rnti, (*itTb).second.harqProcessId);
                          }
                          if (!m_ltePhyUlHarqFeedbackCallback.IsNull ())
                            {
                              m_ltePhyUlHarqFeedbackCallback (harqUlInfo);
                            }
                      }
                    else
                      {
                        std::map <uint16_t, DlInfoListElement_s>::iterator itHarq = harqDlInfoMap.find (tbId.m_rnti);
                        if (itHarq==harqDlInfoMap.end ())
                          {
                            DlInfoListElement_s harqDlInfo;
                            harqDlInfo.m_harqStatus.resize (m_layersNum, DlInfoListElement_s::ACK);
                            harqDlInfo.m_rnti = tbId.m_rnti;
                            harqDlInfo.m_harqProcessId = (*itTb).second.harqProcessId;
                            if ((*itTb).second.corrupt)
                              {
                                harqDlInfo.m_harqStatus.at (tbId.m_layer) = DlInfoListElement_s::NACK;
                                NS_LOG_DEBUG (this << " RNTI " << tbId.m_rnti << " harqId " << (uint16_t)(*itTb).second.harqProcessId << " layer " <<(uint16_t)tbId.m_layer << " send DL-HARQ-NACK");
                                m_harqPhyModule->UpdateDlHarqProcessStatus ((*itTb).second.harqProcessId, tbId.m_layer, (*itTb).second.mi, (*itTb).second.size, (*itTb).second.size / EffectiveCodingRate [(*itTb).second.mcs]);
                              }
                            else
                              {

                                harqDlInfo.m_harqStatus.at (tbId.m_layer) = DlInfoListElement_s::ACK;
                                NS_LOG_DEBUG (this << " RNTI " << tbId.m_rnti << " harqId " << (uint16_t)(*itTb).second.harqProcessId << " layer " <<(uint16_t)tbId.m_layer << " size " << (*itTb).second.size << " send DL-HARQ-ACK");
                                m_harqPhyModule->ResetDlHarqProcessStatus ((*itTb).second.harqProcessId);
                              }
                            harqDlInfoMap.insert (std::pair <uint16_t, DlInfoListElement_s> (tbId.m_rnti, harqDlInfo));
                          }
                        else
                        {
                          if ((*itTb).second.corrupt)
                            {
                              (*itHarq).second.m_harqStatus.at (tbId.m_layer) = DlInfoListElement_s::NACK;
                              NS_LOG_DEBUG (this << " RNTI " << tbId.m_rnti << " harqId " << (uint16_t)(*itTb).second.harqProcessId << " layer " <<(uint16_t)tbId.m_layer << " size " << (*itHarq).second.m_harqStatus.size () << " send DL-HARQ-NACK");
                              m_harqPhyModule->UpdateDlHarqProcessStatus ((*itTb).second.harqProcessId, tbId.m_layer, (*itTb).second.mi, (*itTb).second.size, (*itTb).second.size / EffectiveCodingRate [(*itTb).second.mcs]);
                            }
                          else
                            {
                              NS_ASSERT_MSG (tbId.m_layer < (*itHarq).second.m_harqStatus.size (), " layer " << (uint16_t)tbId.m_layer);
                              (*itHarq).second.m_harqStatus.at (tbId.m_layer) = DlInfoListElement_s::ACK;
                              NS_LOG_DEBUG (this << " RNTI " << tbId.m_rnti << " harqId " << (uint16_t)(*itTb).second.harqProcessId << " layer " << (uint16_t)tbId.m_layer << " size " << (*itHarq).second.m_harqStatus.size () << " send DL-HARQ-ACK");
                              m_harqPhyModule->ResetDlHarqProcessStatus ((*itTb).second.harqProcessId);
                            }
                        }
                      } // end if ((*itTb).second.downlink) HARQ
                  } // end if (!(*itTb).second.harqFeedbackSent)
              }
          }
      }

  // send DL HARQ feedback to LtePhy
  std::map <uint16_t, DlInfoListElement_s>::iterator itHarq;
  for (itHarq = harqDlInfoMap.begin (); itHarq != harqDlInfoMap.end (); itHarq++)
    {
      if (!m_ltePhyDlHarqFeedbackCallback.IsNull ())
        {
          m_ltePhyDlHarqFeedbackCallback ((*itHarq).second);
        }
    }
  // forward control messages of this frame to LtePhy
  if (!m_rxControlMessageList.empty ())
    {
      if (!m_ltePhyRxCtrlEndOkCallback.IsNull ())
        {
          m_ltePhyRxCtrlEndOkCallback (m_rxControlMessageList);
        }
    }
  ChangeState (IDLE);
  m_rxPacketBurstList.clear ();
  m_rxControlMessageList.clear ();
  m_expectedTbs.clear ();
}




void
LteSpectrumPhy::EndRxSlData ()
{
  NS_LOG_FUNCTION (this << " Node ID:" << GetDevice ()->GetNode ()->GetId () << " State: " << m_state);

  //NS_ASSERT (m_state == RX_DATA);

  // this will trigger CQI calculation and Error Model evaluation
  // as a side effect, the error model should update the error status of all TBs
  m_interferenceSl->EndRx ();
  NS_LOG_INFO ("No. of SL bursts " << m_rxPacketInfo.size ());
  NS_LOG_INFO ("Expected TBs (communication) " << m_expectedSlTbs.size ());
  NS_LOG_INFO ("Expected TBs (discovery) " << m_expectedDiscTbs.size ());
  NS_LOG_INFO ("No Ctrl messages " << m_rxControlMessageList.size ());

  NS_ASSERT (m_transmissionMode < m_txModeGain.size ());

  //Compute error on PSSCH
  //Create a mapping between the packet tag and the index of the packet bursts. We need this information to access the right SINR measurement.
  std::map <SlTbId_t, uint32_t> expectedTbToSinrIndex;
  for (uint32_t i = 0; i < m_rxPacketInfo.size (); i++)
    {
      //even though there may be multiple packets, they all have
      //the same tag
      if (m_rxPacketInfo[i].m_rxPacketBurst)   //if data packet
        {
          std::list<Ptr<Packet> >::const_iterator j = m_rxPacketInfo[i].m_rxPacketBurst->Begin ();
          // retrieve TB info of this packet
          LteRadioBearerTag tag;
          (*j)->PeekPacketTag (tag);
          SlTbId_t tbId;
          tbId.m_rnti = tag.GetRnti ();
          tbId.m_l1dst = tag.GetDestinationL2Id () & 0xFF;
          expectedTbToSinrIndex.insert (std::pair<SlTbId_t, uint32_t> (tbId, i));
        }
    }

  std::set<int> collidedRbBitmap;
  if (m_dropRbOnCollisionEnabled)
    {
      NS_LOG_DEBUG (this << " PSSCH DropOnCollisionEnabled: Identifying RB Collisions");
      std::set<int> collidedRbBitmapTemp;
      for (expectedSlTbs_t::iterator itTb = m_expectedSlTbs.begin (); itTb != m_expectedSlTbs.end (); itTb++ )
        {
          for (std::vector<int>::iterator rbIt =  (*itTb).second.rbBitmap.begin (); rbIt != (*itTb).second.rbBitmap.end (); rbIt++)
            {
              if (collidedRbBitmapTemp.find (*rbIt) != collidedRbBitmapTemp.end ())
                {
                  //collision, update the bitmap
                  collidedRbBitmap.insert (*rbIt);
                }
              else
                {
                  //store resources used by the packet to detect collision
                  collidedRbBitmapTemp.insert (*rbIt);
                }
            }
        }

    }

  //Compute the error and check for collision for each expected Tb
  expectedSlTbs_t::iterator itTb = m_expectedSlTbs.begin ();
  std::map <SlTbId_t, uint32_t>::iterator itSinr;
  while (itTb != m_expectedSlTbs.end ())
    {
      itSinr = expectedTbToSinrIndex.find ((*itTb).first);
      // avoid to check for errors and collisions when there is no actual data transmitted
      if ((m_rxPacketInfo.size () > 0) && (itSinr != expectedTbToSinrIndex.end ()))
        {
          HarqProcessInfoList_t harqInfoList;
          bool rbCollided = false;
          if (m_slDataErrorModelEnabled)
            {
              // retrieve HARQ info
              if ((*itTb).second.ndi == 0)
                {
                  harqInfoList = m_slHarqPhyModule->GetHarqProcessInfoSl ((*itTb).first.m_rnti, (*itTb).first.m_l1dst);
                  NS_LOG_DEBUG ("Nb Retx=" << harqInfoList.size ());
                }

              NS_LOG_DEBUG ("Time: " << Simulator::Now ().GetMilliSeconds () << "msec From: " << (*itTb).first.m_rnti << " Corrupt: " << (*itTb).second.corrupt);


              if (m_dropRbOnCollisionEnabled)
                {
                  NS_LOG_DEBUG (this << " PSSCH DropOnCollisionEnabled: Labeling Corrupted TB");
                  //Check if any of the RBs have been decoded
                  for (std::vector<int>::iterator rbIt =  (*itTb).second.rbBitmap.begin (); rbIt != (*itTb).second.rbBitmap.end (); rbIt++)
                    {
                      if (collidedRbBitmap.find (*rbIt) != collidedRbBitmap.end ())
                        {
                          NS_LOG_DEBUG (*rbIt << " collided, labeled as corrupted!");
                          rbCollided = true;
                          (*itTb).second.corrupt = true;
                          break;
                        }
                    }
                }
              TbErrorStats_t tbStats = LteNistErrorModel::GetPsschBler (m_fadingModel,LteNistErrorModel::SISO, (*itTb).second.mcs, GetMeanSinr (m_slSinrPerceived[(*itSinr).second] * m_slRxGain, (*itTb).second.rbBitmap),  harqInfoList);
              (*itTb).second.sinr = tbStats.sinr;
              if (!rbCollided)
                {
                  if (m_slHarqPhyModule->IsPrevDecoded ((*itTb).first.m_rnti, (*itTb).first.m_l1dst))
                    {
                      (*itTb).second.corrupt = false;
                    }
                  else
                    {
                      double rndVal = m_random->GetValue ();
                      (*itTb).second.corrupt = rndVal > tbStats.tbler ? false : true;
                    }
                }

              NS_LOG_DEBUG ("From RNTI " << (*itTb).first.m_rnti << " TB size " << (*itTb).second.size << " MCS " << (uint32_t)(*itTb).second.mcs);
              NS_LOG_DEBUG ("RB bitmap size " << (*itTb).second.rbBitmap.size () << " TBLER " << tbStats.tbler
                                              << " corrupted " << (*itTb).second.corrupt << " prevDecoded"
                                              << m_slHarqPhyModule->IsPrevDecoded ((*itTb).first.m_rnti, (*itTb).first.m_l1dst));

            }
          else
            {
              if (m_dropRbOnCollisionEnabled)
                {
                  NS_LOG_DEBUG (this << " PSSCH DropOnCollisionEnabled: Labeling Corrupted TB");
                  //Check if any of the RBs have been decoded
                  for (std::vector<int>::iterator rbIt =  (*itTb).second.rbBitmap.begin (); rbIt != (*itTb).second.rbBitmap.end (); rbIt++)
                    {
                      if (collidedRbBitmap.find (*rbIt) != collidedRbBitmap.end ())
                        {
                          NS_LOG_DEBUG (*rbIt << " collided, labeled as corrupted!");
                          rbCollided = true;
                          (*itTb).second.corrupt = true;
                          break;
                        }
                    }
                }

              if (!rbCollided)
                {
                  (*itTb).second.corrupt = false;
                }
            }


          // fire traces on SL reception PHY stats
          PhyReceptionStatParameters params;
          params.m_timestamp = Simulator::Now ().GetMilliSeconds ();
          params.m_cellId = m_cellId;
          params.m_imsi = 0;   // it will be set by DlPhyTransmissionCallback in LteHelper
          params.m_rnti = (*itTb).first.m_rnti;
          params.m_txMode = m_transmissionMode;
          params.m_layer =  0;
          params.m_mcs = (*itTb).second.mcs;
          params.m_size = (*itTb).second.size;
          params.m_rv = (*itTb).second.rv;
          params.m_ndi = (*itTb).second.ndi;
          params.m_correctness = (uint8_t) !(*itTb).second.corrupt;
          params.m_sinrPerRb = GetMeanSinr (m_slSinrPerceived[(*itSinr).second] * m_slRxGain, (*itTb).second.rbBitmap);
          params.m_rv = harqInfoList.size ();
          m_slPhyReception (params);
        }

      itTb++;
    }


  for (uint32_t i = 0; i < m_rxPacketInfo.size (); i++)
    {
      //even though there may be multiple packets, they all have
      //the same tag
      if (m_rxPacketInfo[i].m_rxPacketBurst)   //if data packet
        {
          for (std::list<Ptr<Packet> >::const_iterator j = m_rxPacketInfo[i].m_rxPacketBurst->Begin ();
               j != m_rxPacketInfo[i].m_rxPacketBurst->End (); ++j)
            {
              // retrieve TB info of this packet
              LteRadioBearerTag tag;
              (*j)->PeekPacketTag (tag);
              SlTbId_t tbId;
              tbId.m_rnti = tag.GetRnti ();
              tbId.m_l1dst = tag.GetDestinationL2Id () & 0xFF;
              itTb = m_expectedSlTbs.find (tbId);
              NS_LOG_INFO ("Packet of " << tbId.m_rnti << " group " <<  (uint16_t) tbId.m_l1dst);
              if (itTb != m_expectedSlTbs.end ())
                {
                  m_slHarqPhyModule->IncreaseTbIdx ((*itTb).first.m_rnti, (*itTb).first.m_l1dst);
                  if (!(*itTb).second.corrupt && !m_slHarqPhyModule->IsPrevDecoded ((*itTb).first.m_rnti, (*itTb).first.m_l1dst))
                    {
                      m_slHarqPhyModule->IndicatePrevDecoded ((*itTb).first.m_rnti, (*itTb).first.m_l1dst);
                      m_phyRxEndOkTrace (*j);

                      if (!m_ltePhyRxDataEndOkCallback.IsNull ())
                        {
                          m_ltePhyRxDataEndOkCallback (*j);
                        }
                    }
                  else
                    {
                      // TB received with errors
                      m_phyRxEndErrorTrace (*j);
                    }

                  //update HARQ information
                  //because we do not have feedbacks we do not reset HARQ now, even if packet was
                  //Successfully received
                  m_slHarqPhyModule->UpdateSlHarqProcessStatus (tbId.m_rnti, tbId.m_l1dst, (*itTb).second.sinr);
                }
            }
        }
    }


  /* Currently the MIB-SL is treated as a control message. Thus, the following logic applies also to the MIB-SL
   * The differences: calculation of BLER */
  // When control messages collide in the PSCCH, the receiver cannot know how many transmissions occurred
  // we sort the messages by SINR and try to decode the ones with highest average SINR per RB first
  // only one message per RB can be decoded

  std::list<Ptr<LteControlMessage> > rxControlMessageOkList;
  bool error = true;
  bool ctrlMessageFound = false;
  std::multiset<SlCtrlPacketInfo_t> sortedControlMessages;
  //container to store the RB indices of the collided TBs
  collidedRbBitmap.clear ();
  //container to store the RB indices of the decoded TBs
  std::set<int> rbDecodedBitmap;

  for (uint32_t i = 0; i < m_rxPacketInfo.size (); i++)
    {
      //if control packet
      if (m_rxPacketInfo[i].m_rxControlMessage && m_rxPacketInfo[i].m_rxControlMessage->GetMessageType () != LteControlMessage::SL_DISC_MSG)
        {
          double meanSinr = GetMeanSinr (m_slSinrPerceived[i], m_rxPacketInfo[i].rbBitmap);
          SlCtrlPacketInfo_t pInfo;
          pInfo.sinr = meanSinr;
          pInfo.index = i;
          sortedControlMessages.insert (pInfo);
        }
    }

  if (m_dropRbOnCollisionEnabled)
    {
      NS_LOG_DEBUG (this << "Ctrl DropOnCollisionEnabled");
      //Add new loop to make one pass and identify which RB have collisions
      std::set<int> collidedRbBitmapTemp;

      for (std::multiset<SlCtrlPacketInfo_t>::iterator it = sortedControlMessages.begin (); it != sortedControlMessages.end (); it++ )
        {
          int i = (*it).index;
          for (std::vector<int>::iterator rbIt =  m_rxPacketInfo[i].rbBitmap.begin (); rbIt != m_rxPacketInfo[i].rbBitmap.end (); rbIt++)
            {
              if (collidedRbBitmapTemp.find (*rbIt) != collidedRbBitmapTemp.end ())
                {
                  //collision, update the bitmap
                  collidedRbBitmap.insert (*rbIt);
                  break;
                }
              else
                {
                  //store resources used by the packet to detect collision
                  collidedRbBitmapTemp.insert ((*rbIt));
                }
            }
        }
    }

  //uint32_t txFeedbackValue = 0;

  for (std::multiset<SlCtrlPacketInfo_t>::iterator it = sortedControlMessages.begin (); it != sortedControlMessages.end (); it++ )
    {
      int i = (*it).index;

      bool corrupt = false;
      bool weakSignal = false;
      ctrlMessageFound = true;
      uint32_t conflict = false;
      bool first = true;
      if (m_slCtrlErrorModelEnabled)
        {
          for (std::vector<int>::iterator rbIt =  m_rxPacketInfo[i].rbBitmap.begin ();  rbIt != m_rxPacketInfo[i].rbBitmap.end (); rbIt++)
            {
              //if m_dropRbOnCollisionEnabled == false, collidedRbBitmap will remain empty
              //and we move to the second "if" to check if the TB with similar RBs has already
              //been decoded. If m_dropRbOnCollisionEnabled == true, all the collided TBs
              //are marked corrupt and this for loop will break in the first "if" condition
              if (collidedRbBitmap.find (*rbIt) != collidedRbBitmap.end ())
                {
                  corrupt = true;
                  NS_LOG_DEBUG (this << " RB " << *rbIt << " has collided");
                  break;
                }
              if (rbDecodedBitmap.find (*rbIt) != rbDecodedBitmap.end ())
                {
                  NS_LOG_INFO (*rbIt << " TB with the similar RB has already been decoded. Avoid to decode it again!");
                  corrupt = true;
                  first = false;
                  conflict = true;
                  break;
                }
             }

          if (!corrupt)
            {
              double errorRate;
              double weakSignalTest;
              double conflictTest;
              //double lowEnergyTest;
              //bool isCase1 = false;
              if (m_rxPacketInfo[i].m_rxControlMessage->GetMessageType () == LteControlMessage::SCI)
                {
                  NS_LOG_INFO (this << " Average gain for SIMO = " << m_slRxGain << " Watts");
                  weakSignalTest = LteNistErrorModel::GetPscchBler (m_fadingModel,LteNistErrorModel::SISO, GetMeanSinr (m_slInterferencePerceived[i] * m_slRxGain, m_rxPacketInfo[i].rbBitmap)).tbler;
                  weakSignal = m_random->GetValue () > weakSignalTest ? false : true;
                  if (weakSignal)
                    {
                      conflict = false;
                    }
                  else if (!weakSignal && !conflict)
                    {
                      conflictTest = LteNistErrorModel::GetPscchBler (m_fadingModel,LteNistErrorModel::SISO, GetMeanSinr (m_slSinrPerceived[i] * m_slRxGain, m_rxPacketInfo[i].rbBitmap)).tbler;
                      conflict = m_random->GetValue () > conflictTest ? false :true;
                    }

                  NS_LOG_INFO (this << " PSCCH Decoding, weakSignalTest " << weakSignalTest << " error " << corrupt);
                }
              else if (m_rxPacketInfo[i].m_rxControlMessage->GetMessageType () == LteControlMessage::MIB_SL)
                {
                  //Average gain for SIMO based on [CatreuxMIMO] --> m_slSinrPerceived[i] * 2.51189
                  errorRate = LteNistErrorModel::GetPsbchBler (m_fadingModel,LteNistErrorModel::SISO, GetMeanSinr (m_slSinrPerceived[i] * m_slRxGain, m_rxPacketInfo[i].rbBitmap)).tbler;
                  corrupt = m_random->GetValue () > errorRate ? false : true;
                  NS_LOG_INFO (this << " PSBCH Decoding, errorRate " << errorRate << " error " << corrupt);
                }
              else
                {
                  NS_LOG_DEBUG (this << " Unknown SL control message ");
                }
            }
        }
      else
        {
          //No error model enabled. If m_dropRbOnCollisionEnabled == true, it will just label the TB as
          //corrupted if the two TBs received at the same time use same RB. Note: PSCCH occupies one RB.
          //On the other hand, if m_dropRbOnCollisionEnabled == false, all the TBs are considered as not corrupted.
          if (m_dropRbOnCollisionEnabled)
            {
              for (std::vector<int>::iterator rbIt =  m_rxPacketInfo[i].rbBitmap.begin ();  rbIt != m_rxPacketInfo[i].rbBitmap.end (); rbIt++)
                {
                  if (collidedRbBitmap.find (*rbIt) != collidedRbBitmap.end ())
                    {
                      corrupt = true;
                      NS_LOG_DEBUG (this << " RB " << *rbIt << " has collided");
                      break;
                    }
                }
            }
        }
      
      m_isDecoded = false;
      if (!weakSignal && !conflict)
        {
          error = false;       //at least one control packet is OK
          m_isDecoded = true;
          rxControlMessageOkList.push_back (m_rxPacketInfo[i].m_rxControlMessage);
          //Store the indices of the decoded RBs
          rbDecodedBitmap.insert ( m_rxPacketInfo[i].rbBitmap.begin (), m_rxPacketInfo[i].rbBitmap.end ());
        }

      if (m_rxPacketInfo[i].m_rxControlMessage->GetMessageType () == LteControlMessage::SCI)
        {

          // Add PSCCH trace.
          NS_ASSERT (m_rxPacketInfo[i].m_rxControlMessage->GetMessageType () == LteControlMessage::SCI);
          Ptr<SciLteControlMessage> msg2 = DynamicCast<SciLteControlMessage> (m_rxPacketInfo[i].m_rxControlMessage);
          SciF0ListElement_s scif0 = msg2->GetSciF0 ();
          SciF1ListElement_s scif1 = msg2->GetSciF1 ();


          SlPhyReceptionStatParameters params;
          params.m_timestamp = Simulator::Now ().GetMilliSeconds ();
          params.m_cellId = m_cellId;
          params.m_imsi = 0;       // it will be set by DlPhyTransmissionCallback in LteHelper
          params.m_rnti = scif0.m_rnti;
          params.m_mcs = scif0.m_mcs;
          params.m_size = scif0.m_tbSize;
          params.m_rbStart = scif0.m_rbStart;
          params.m_rbLen = scif0.m_rbLen;
          params.m_resPscch = scif0.m_resPscch;
          params.m_groupDstId = scif0.m_groupDstId;
          params.m_iTrp = scif0.m_trp;
          params.m_hopping = scif0.m_hopping;
          params.m_correctness = (uint8_t) !error;
          params.m_conflict = conflict;
          params.m_weakSignal = weakSignal;

          params.m_priority = scif1.m_priority;
          params.m_rnti = scif1.m_rnti;
          params.m_resReserve = scif1.m_resReserve;
          params.m_frl = scif1.m_frl;
          params.m_timeGap = scif1.m_timeGap;
          params.m_reIndex = scif1.m_reIndex;
          params.m_tbSize = scif1.m_tbSize;
          params.m_frameNo = scif1.m_frameNo;
          params.m_subframeNo = scif1.m_subframeNo;

          params.m_rxPosX = m_mobility->GetPosition ().x;
          params.m_rxPosY = m_mobility->GetPosition ().y;
          params.m_txPosX = m_nodeList.Get(params.m_rnti-1) -> GetObject<MobilityModel> () -> GetPosition ().x;
          params.m_txPosY = m_nodeList.Get(params.m_rnti-1) -> GetObject<MobilityModel> () -> GetPosition ().y;

          double deltaX = params.m_rxPosX - params.m_txPosX;
          double deltaY = params.m_rxPosY - params.m_txPosY;
          double distRxTx = std::sqrt(deltaX * deltaX + deltaY * deltaY);
          params.m_neighbor = 0;
          params.m_isTx = (uint8_t) isTx;
          params.m_nextTxTime = m_nextTxTime + 4;

          m_txID = params.m_rnti;
          if (params.m_nextTxTime == params.m_timestamp)
            {
              m_isDecoded = false;

              // Half-Duplex
              isTx = true;
              params.m_isTx = (uint8_t) isTx;
              params.m_correctness = 0;
              
              // Full-Duplex
              //isTx = false;
              //params.m_isTx = (uint8_t) isTx;
              
            }
          else
            {
              isTx = false;
              params.m_isTx = (uint8_t) isTx;
            }

          uint32_t notReceptType = 0; // (0: recept ok, 1: weak signal, 2: resource confliction, 3: half duplex, 4: unknown)
          if (params.m_correctness)
            {
              notReceptType = 0;
            }
          else if(weakSignal)
            {
              notReceptType = 1;
            }
          else if(conflict)
            {
              notReceptType = 2;
            }
          else if(isTx)
            {
              notReceptType = 3;
            }
          else
            {
              notReceptType = 4;
            }

          params.m_rxType = notReceptType;

          if (distRxTx < 150.0)
            {
              params.m_neighbor = 1;
              if (m_msgLastReception[params.m_rnti-1] == 0)
                {
                  params.m_msgInterval = 0;
                  m_msgLastReception[params.m_rnti-1] = params.m_timestamp;
                }
              else
                {
                  params.m_msgInterval = params.m_timestamp - m_msgLastReception[params.m_rnti-1];
                  if (params.m_correctness)
                    {
                      m_msgLastReception[params.m_rnti-1] = params.m_timestamp;
                    }
                }
              
              if (params.m_rxPosX < 100000 && params.m_rxPosY < 100000 && params.m_txPosX < 100000 && params.m_txPosY < 100000)
                {
                  m_slPscchReception (params);
                }
            }
          else
            {
              m_msgLastReception[params.m_rnti-1] = 0;
              params.m_neighbor = 0;
            }
                  
          // Call trace
          //m_slPscchReception (params);
        }
      if (first && !isTx)
        {
          UpdateRssiRsrpMap(i);
        }
    }

  if (ctrlMessageFound)
    {
      if (!error)
        {
          if (!m_ltePhyRxCtrlEndOkCallback.IsNull ())
            {
              NS_LOG_INFO ("Receive OK (No Error, No Collision)");
              m_ltePhyRxCtrlEndOkCallback (rxControlMessageOkList);
            }
        }
      else
        {
          if (!m_ltePhyRxCtrlEndErrorCallback.IsNull ())
            {
              NS_LOG_INFO (this << " PSCCH Error");
              m_ltePhyRxCtrlEndErrorCallback ();
            }
        }
    }
  
  if (error)
    {
      NS_LOG_INFO (this << " RX ERROR");
    }
  //Sidelink Discovery
  RxDiscovery();

  //done with Sidelink data, control and discovery
  ChangeState (IDLE);
  m_rxPacketBurstList.clear ();
  m_rxControlMessageList.clear ();
  m_rxPacketInfo.clear ();
  m_expectedSlTbs.clear ();
  m_expectedDiscTbs.clear ();
}


void
LteSpectrumPhy::EndRxDlCtrl ()
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC (this << " State: " << m_state);
  
  NS_ASSERT (m_state == RX_DL_CTRL);
  
  // this will trigger CQI calculation and Error Model evaluation
  // as a side effect, the error model should update the error status of all TBs
  m_interferenceCtrl->EndRx ();
  // apply transmission mode gain
  NS_LOG_DEBUG (this << " txMode " << (uint16_t)m_transmissionMode << " gain " << m_txModeGain.at (m_transmissionMode));
  NS_ASSERT (m_transmissionMode < m_txModeGain.size ());
  if (m_transmissionMode>0)
    {
      // in case of MIMO, ctrl is always txed as TX diversity
      m_sinrPerceived *= m_txModeGain.at (1);
    }
//   m_sinrPerceived *= m_txModeGain.at (m_transmissionMode);
  bool error = false;
  if (m_ctrlErrorModelEnabled)
    {
      double  errorRate = LteMiErrorModel::GetPcfichPdcchError (m_sinrPerceived);
      error = m_random->GetValue () > errorRate ? false : true;
      NS_LOG_DEBUG (this << " PCFICH-PDCCH Decodification, errorRate " << errorRate << " error " << error);
    }

  if (!error)
    {
      if (!m_ltePhyRxCtrlEndOkCallback.IsNull ())
        {
          NS_LOG_DEBUG (this << " PCFICH-PDCCH Rxed OK");
          m_ltePhyRxCtrlEndOkCallback (m_rxControlMessageList);
        }
    }
  else
    {
      if (!m_ltePhyRxCtrlEndErrorCallback.IsNull ())
        {
          NS_LOG_DEBUG (this << " PCFICH-PDCCH Error");
          m_ltePhyRxCtrlEndErrorCallback ();
        }
    }
  ChangeState (IDLE);
  m_rxControlMessageList.clear ();
}

void
LteSpectrumPhy::EndRxUlSrs ()
{
  NS_ASSERT (m_state == RX_UL_SRS);
  ChangeState (IDLE);
  m_interferenceCtrl->EndRx ();
  // nothing to do (used only for SRS at this stage)
}

void 
LteSpectrumPhy::SetCellId (uint16_t cellId)
{
  m_cellId = cellId;
}

void
LteSpectrumPhy::AddL1GroupId (uint8_t groupId)
{
  NS_LOG_FUNCTION (this << (uint16_t) groupId);
  m_l1GroupIds.insert(groupId);
}

void
LteSpectrumPhy::RemoveL1GroupId (uint8_t groupId)
{
  m_l1GroupIds.erase (groupId);
}

void
LteSpectrumPhy::SetComponentCarrierId (uint8_t componentCarrierId)
{
  m_componentCarrierId = componentCarrierId;
}

void
LteSpectrumPhy::AddRsPowerChunkProcessor (Ptr<LteChunkProcessor> p)
{
  m_interferenceCtrl->AddRsPowerChunkProcessor (p);
}

void
LteSpectrumPhy::AddDataPowerChunkProcessor (Ptr<LteChunkProcessor> p)
{
  m_interferenceData->AddRsPowerChunkProcessor (p);
}

void
LteSpectrumPhy::AddDataSinrChunkProcessor (Ptr<LteChunkProcessor> p)
{
  m_interferenceData->AddSinrChunkProcessor (p);
}

void
LteSpectrumPhy::AddInterferenceCtrlChunkProcessor (Ptr<LteChunkProcessor> p)
{
  m_interferenceCtrl->AddInterferenceChunkProcessor (p);
}

void
LteSpectrumPhy::AddInterferenceDataChunkProcessor (Ptr<LteChunkProcessor> p)
{
  m_interferenceData->AddInterferenceChunkProcessor (p);
}

void
LteSpectrumPhy::AddCtrlSinrChunkProcessor (Ptr<LteChunkProcessor> p)
{
  m_interferenceCtrl->AddSinrChunkProcessor (p);
}

void
LteSpectrumPhy::AddSlSinrChunkProcessor (Ptr<LteSlChunkProcessor> p)
{
  m_interferenceSl->AddSinrChunkProcessor (p);
}

void
LteSpectrumPhy::AddSlSignalChunkProcessor (Ptr<LteSlChunkProcessor> p)
{
  m_interferenceSl->AddRsPowerChunkProcessor (p);
}

void
LteSpectrumPhy::AddSlInterferenceChunkProcessor (Ptr<LteSlChunkProcessor> p)
{
  m_interferenceSl->AddInterferenceChunkProcessor (p);
}

void 
LteSpectrumPhy::SetTransmissionMode (uint8_t txMode)
{
  NS_LOG_FUNCTION (this << (uint16_t) txMode);
  NS_ASSERT_MSG (txMode < m_txModeGain.size (), "TransmissionMode not available: 1.." << m_txModeGain.size ());
  m_transmissionMode = txMode;
  m_layersNum = TransmissionModesLayers::TxMode2LayerNum (txMode);
}


void 
LteSpectrumPhy::SetTxModeGain (uint8_t txMode, double gain)
{
  NS_LOG_FUNCTION (this << " Txmode " << (uint16_t)txMode << " gain " << gain);
  // convert to linear
  gain = std::pow (10.0, (gain / 10.0));
  if (m_txModeGain.size () < txMode)
  {
    m_txModeGain.resize (txMode);
  }
  std::vector <double> temp;
  temp = m_txModeGain;
  m_txModeGain.clear ();
  for (uint8_t i = 0; i < temp.size (); i++)
  {
    if (i==txMode-1)
    {
      m_txModeGain.push_back (gain);
    }
    else
    {
      m_txModeGain.push_back (temp.at (i));
    }
  }
}

std::vector<uint32_t>
LteSpectrumPhy::GetFeedbackProvidedResources(uint32_t subChannel, uint32_t subFrame, uint32_t nFeedback, uint32_t totalRU)
{
  NS_LOG_FUNCTION (this);
  std::vector<uint32_t> feedback_RUs;
  for (uint32_t i = 0; i < nFeedback; i++)
    {
      uint32_t feedback_ru = ((subChannel * subFrame) + i) % totalRU;
      feedback_RUs.push_back(feedback_ru);
    }

  return feedback_RUs;
}

std::vector<std::vector<bool>>
LteSpectrumPhy::GetDecodingMap()
{
  NS_LOG_FUNCTION (this);
  return m_decodingMap;
}

std::vector<std::vector<double>>
LteSpectrumPhy::GetRssiMap ()
{
  NS_LOG_FUNCTION (this);
  return m_rssiMap;
}

std::vector<std::vector<double>>
LteSpectrumPhy::GetRsrpMap ()
{
  NS_LOG_FUNCTION (this);
  return m_rsrpMap;
}

void
LteSpectrumPhy::MoveSensingWindow(uint32_t sIdx, uint32_t scPeriod)
{
  NS_LOG_FUNCTION (this);
  uint32_t nSubChannel = std::ceil(50 / m_RbPerSubChannel);

  for (uint32_t idx_sc = 0; idx_sc < nSubChannel; idx_sc++)
  {
    for (uint32_t idx_sf = sIdx; idx_sf < sIdx + scPeriod; idx_sf++)
      {
        m_rssiMap[idx_sc][idx_sf%1000] = 0;
        m_rsrpMap[idx_sc][idx_sf%1000] = 0;
        m_decodingMap[idx_sc][idx_sf&1000] = false;
      }
  }
}

void
LteSpectrumPhy::SetNextTxTime(uint32_t txTime)
{
  m_nextTxTime = txTime;
}

void
LteSpectrumPhy::UpdateRssiRsrpMap (int sigIndex)
{
  NS_LOG_FUNCTION (this);
  uint16_t rbNum = 0;
  double rssiSum = 0.0;
  double rssi = 0.0;
  double rsrpSum = 0.0;
  double rsrp_dBm;
      
  Values::const_iterator itIntN = m_slInterferencePerceived[sigIndex].ConstValuesBegin ();
  Values::const_iterator itPj = m_slSignalPerceived[sigIndex].ConstValuesBegin ();
  for(itPj = m_slSignalPerceived[sigIndex].ConstValuesBegin ();
      itPj != m_slSignalPerceived[sigIndex].ConstValuesEnd ();
      itIntN++, itPj++)
    {
      rbNum++;
      double interfPlusNoisePowerTxW = ((*itIntN) * 180000.0) / 12.0;
      double signalPowerTxW = ((*itPj) * 180000.0) / 12.0;
      rsrpSum += signalPowerTxW;
      rssiSum += (2 * (interfPlusNoisePowerTxW + signalPowerTxW));
    }
         
  int subChannel = std::ceil(m_slRxRbStartIdx / m_RbPerSubChannel);
  int64_t subFrame = Simulator::Now ().GetMilliSeconds () % 1000;

  rssi = rssiSum / (double)rbNum;
  m_rssiMap[subChannel][subFrame] = rssi;

  rsrp_dBm = 10 * log10 (1000 * (rsrpSum / static_cast<double> (rbNum)));
  m_rsrpMap[subChannel][subFrame] = rsrp_dBm;

  m_decodingMap[subChannel][subFrame] = m_isDecoded;
  Ptr<LteUeNetDevice> lteDevice = DynamicCast<LteUeNetDevice> (m_device);
  if (lteDevice->GetImsi () == 5)
    {
      NS_LOG_DEBUG("TxID = " << m_txID+3 << ", RX subChannel = " << subChannel <<", subFrame = " << subFrame);
    }
}

/*void
LteSpectrumPhy::UpdateTxFeedbackMap (uint32_t value)
{
  NS_LOG_FUNCTION (this);
  int subChannel = std::ceil(m_slRxRbStartIdx / 15);
  int64_t subFrame = Simulator::Now ().GetMilliSeconds () % 48;
  
  m_txFeedbackMap[subChannel][subFrame] = value;
}*/

double
LteSpectrumPhy::GetMeanSinr (const SpectrumValue& sinr, const std::vector<int>& map)
{
  NS_LOG_FUNCTION (this <<sinr);
  SpectrumValue sinrCopy = sinr;
  double sinrLin = 0;
  for (uint32_t i = 0; i < map.size (); i++)
    {
      sinrLin += sinrCopy[map.at (i)];
    }
  return sinrLin / map.size();
}

LteSpectrumPhy::State
LteSpectrumPhy::GetState ()
{
  NS_LOG_FUNCTION (this);
  return m_state;
}

void
LteSpectrumPhy::SetSlssid (uint64_t slssid)
{
  NS_LOG_FUNCTION (this);
  m_slssId = slssid;
}

void
LteSpectrumPhy::SetRxPool (Ptr<SidelinkDiscResourcePool> newpool)
{
  NS_LOG_FUNCTION (this);
  m_discRxPools.push_back (newpool);
}

void
LteSpectrumPhy::AddDiscTxApps (std::list<uint32_t> apps)
{
  NS_LOG_FUNCTION (this);
  m_discTxApps = apps;
}

void
LteSpectrumPhy::AddDiscRxApps (std::list<uint32_t> apps)
{
  NS_LOG_FUNCTION (this);
  m_discRxApps = apps;
}

bool
LteSpectrumPhy::FilterRxApps (SlDiscMsg disc)
{
  NS_LOG_FUNCTION (this << disc.m_proSeAppCode);
  bool exist = false;
  for (std::list<uint32_t>::iterator it = m_discRxApps.begin (); it != m_discRxApps.end (); ++it)
    {
      if ((std::bitset <184>)*it == disc.m_proSeAppCode)
        {
          exist = true;
        }
    }
  return exist;
}

void
LteSpectrumPhy::SetDiscNumRetx (uint8_t retx)
{
  NS_LOG_FUNCTION (this << retx);
  m_slHarqPhyModule->SetDiscNumRetx (retx);
}

void
LteSpectrumPhy::SetSlRxGain (double gain)
{
  NS_LOG_FUNCTION (this << gain);
  // convert to linear
  gain = std::pow (10.0, (gain / 10.0));
  NS_LOG_DEBUG("Linear gain = "<<gain);
  m_slRxGain = gain;
}

int64_t
LteSpectrumPhy::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_random->SetStream (stream);
  return 1;
}

void
LteSpectrumPhy::RxDiscovery ()
{
  //Sidelink Discovery
  NS_LOG_FUNCTION (this);
  bool foundDiscoveryMsg = false;
  std::map<SlDiscTbId_t, uint32_t> expectedTbToSinrDiscIndex;
  for (uint32_t i = 0; i < m_rxPacketInfo.size (); i++)
    {
      //if there is control and it is discovery
      if (m_rxPacketInfo[i].m_rxControlMessage && m_rxPacketInfo[i].m_rxControlMessage->GetMessageType () == LteControlMessage::SL_DISC_MSG)
        {
          //data should not be included
          NS_ABORT_MSG_IF (m_rxPacketInfo[i].m_rxPacketBurst != 0, "Discovery message should not carry the data packets");
          foundDiscoveryMsg = true;
          Ptr<LteControlMessage> rxCtrlMsg = m_rxPacketInfo[i].m_rxControlMessage;
          Ptr<SlDiscMessage> msg = DynamicCast<SlDiscMessage> (rxCtrlMsg);
          SlDiscMsg disc = msg->GetSlDiscMessage ();
          bool exist = FilterRxApps (disc);
          if (exist)
            {
              // retrieve TB info of this packet
              SlDiscTbId_t tbId;
              tbId.m_rnti = disc.m_rnti;
              tbId.m_resPsdch = disc.m_resPsdch;
              expectedTbToSinrDiscIndex.insert (std::pair<SlDiscTbId_t, uint8_t> (tbId, i));

              std::list<Ptr<SidelinkDiscResourcePool> >::iterator discIt;
              uint16_t txCount = 0;
              std::vector <int> rbMap;
              for (discIt = m_discRxPools.begin (); discIt != m_discRxPools.end (); discIt++)
                {
                  std::list<SidelinkDiscResourcePool::SidelinkTransmissionInfo> m_psdchTx = (*discIt)->GetPsdchTransmissions (disc.m_resPsdch);
                  NS_LOG_DEBUG (this << " Total number of discovery transmissions = " << m_psdchTx.size ());
                  std::list<SidelinkDiscResourcePool::SidelinkTransmissionInfo>::iterator txIt = m_psdchTx.begin ();
                  if (txIt != m_psdchTx.end ())
                    {
                      //There can be more than one (max 4) PSDCH transmissions, therefore, we need to
                      //match the RBs of all the possible PSDCH with the RBs of the received discovery
                      //message. This way will make sure that we construct the correct RB map
                      while (txIt != m_psdchTx.end ())
                        {
                          for (int i = txIt->rbStart; i < txIt->rbStart + txIt->nbRb; i++)
                            {
                              NS_LOG_LOGIC (this << " Checking PSDCH RB " << i);
                              rbMap.push_back (i);
                            }
                          if (m_rxPacketInfo [i].rbBitmap == rbMap)
                            {
                              //Here, it may happen that the first transmission and the retransmission is
                              //on the identical RBs but different subframes. If this happens, this while
                              //loop will break on txCount == 1 (Note, we don't have access to the subframe
                              //number here). The code starting with if (m_psdchTx.size () > 1)
                              //will take care of such conditions.
                              txCount++;
                              NS_LOG_DEBUG (this << " PSDCH RB matched");
                              break;
                            }
                          else
                            {
                              rbMap.clear ();
                            }

                          txIt++;
                        }

                      //If there are retransmissions we need to keep track of all the transmissions to properly
                      //compute the NDI and RV.
                      if (m_psdchTx.size () > 1)
                        {
                          std::map <uint16_t, uint16_t>::iterator it;
                          it = m_slDiscTxCount.find (disc.m_rnti);
                          if (it == m_slDiscTxCount.end ())
                            {
                              m_slDiscTxCount.insert (std::pair <uint16_t, uint16_t > (disc.m_rnti, 1));
                            }
                          else
                            {
                              it->second++;
                              txCount = it->second;
                              NS_LOG_DEBUG ("It is a Retransmission. Transmission count = " << txCount);
                            }

                          if (it->second == m_psdchTx.size ())
                            {
                              NS_LOG_DEBUG ("We reached the maximum Transmissions (Tx + ReTx) = " << txCount);
                              it->second = 0;
                            }
                        }
                      NS_LOG_DEBUG (this << " PSDCH transmission " << txCount);
                      //reception
                      NS_LOG_DEBUG (this << " Expecting PSDCH reception on PSDCH resource " << (uint16_t) (disc.m_resPsdch));
                      NS_ABORT_MSG_IF (txCount == 0, "PSDCH txCount should be greater than zero");
                      uint8_t rv = txCount - 1;
                      NS_ABORT_MSG_IF (rv > m_psdchTx.size (), " RV number can not be greater than total number of transmissions");
                      bool ndi = txCount == 1 ? true : false;
                      NS_LOG_DEBUG (this << " Adding expected TB.");
                      AddExpectedTb (disc.m_rnti, disc.m_resPsdch, ndi, rbMap, rv);
                      txCount = 0;
                      rbMap.clear ();
                    }
                }
            }
        }
    }

  //container to store the RB indices of the collided TBs
  std::set<int> collidedRbBitmap;
  //container to store the RB indices of the decoded TBs
  std::set<int> rbDecodedBitmap;
  std::set<SlCtrlPacketInfo_t> sortedDiscMessages;
  std::map<SlDiscTbId_t, uint32_t>::iterator itSinrDisc;

  for (expectedDiscTbs_t::iterator it = m_expectedDiscTbs.begin (); it != m_expectedDiscTbs.end (); it++)
    {
      itSinrDisc = expectedTbToSinrDiscIndex.find ((*it).first);
      double meanSinr = GetMeanSinr (m_slSinrPerceived[(*itSinrDisc).second], (*it).second.rbBitmap);
      SlCtrlPacketInfo_t pInfo;
      pInfo.sinr = meanSinr;
      pInfo.index = (*itSinrDisc).second;
      sortedDiscMessages.insert (pInfo);
      NS_LOG_DEBUG ("sortedDiscMessages size = "<<sortedDiscMessages.size() << " SINR = " << pInfo.sinr << " Index = " << pInfo.index);
    }

  if (m_dropRbOnCollisionEnabled)
    {
      NS_LOG_DEBUG (this << " PSDCH DropOnCollisionEnabled: Identifying RB Collisions");
      std::set<int> collidedRbBitmapTemp;
      for (expectedDiscTbs_t::iterator itDiscTb = m_expectedDiscTbs.begin (); itDiscTb != m_expectedDiscTbs.end (); itDiscTb++)
        {
          for (std::vector<int>::iterator rbIt = (*itDiscTb).second.rbBitmap.begin (); rbIt != (*itDiscTb).second.rbBitmap.end (); rbIt++)
            {
              if (collidedRbBitmapTemp.find (*rbIt) != collidedRbBitmapTemp.end ())
                {
                  //collision, update the bitmap
                  NS_LOG_DEBUG ("Collided RB " << (*rbIt));
                  collidedRbBitmap.insert (*rbIt);
                }
              else
                {
                  //store resources used by the packet to detect collision
                  collidedRbBitmapTemp.insert (*rbIt);
                }
            }
        }
    }

  std::list<Ptr<LteControlMessage> > rxDiscMessageOkList;
  expectedDiscTbs_t::iterator itTbDisc = m_expectedDiscTbs.begin ();
  HarqProcessInfoList_t harqInfoList;

  for (std::set<SlCtrlPacketInfo_t>::iterator it = sortedDiscMessages.begin (); it != sortedDiscMessages.end (); it++)
    {
      int i = (*it).index;
      NS_LOG_DEBUG ("Decoding.." << " starting from index = " << i);
      Ptr<LteControlMessage> rxCtrlMsg = m_rxPacketInfo[i].m_rxControlMessage;
      Ptr<SlDiscMessage> msg = DynamicCast<SlDiscMessage> (rxCtrlMsg);
      SlDiscMsg disc = msg->GetSlDiscMessage ();
      SlDiscTbId_t tbId;
      tbId.m_rnti = disc.m_rnti;
      tbId.m_resPsdch = disc.m_resPsdch;
      itTbDisc =  m_expectedDiscTbs.find (tbId);

      itSinrDisc = expectedTbToSinrDiscIndex.find ((*itTbDisc).first);
      NS_ABORT_MSG_IF ((itSinrDisc == expectedTbToSinrDiscIndex.end ()), " Unable to retrieve SINR of the expected TB");
      NS_LOG_DEBUG ("SINR value index of this TB in m_slSinrPerceived vector is " << (*itSinrDisc).second);
      // avoid to check for errors when error model is not enabled
      if (m_slDiscoveryErrorModelEnabled)
        {
          // retrieve HARQ info
          if ((*itTbDisc).second.ndi == 0)
            {
              harqInfoList = m_slHarqPhyModule->GetHarqProcessInfoDisc ((*itTbDisc).first.m_rnti,(*itTbDisc).first.m_resPsdch);
              NS_LOG_DEBUG (this << " Number of Retx =" << harqInfoList.size ());
            }

              //Check if any of the RBs in this TB have been collided
              for (std::vector<int>::iterator rbIt =  (*itTbDisc).second.rbBitmap.begin (); rbIt != (*itTbDisc).second.rbBitmap.end (); rbIt++)
                {
                  //if m_dropRbOnCollisionEnabled == false, collidedRbBitmap will remain empty
                  //and we move to the second "if" to check if the TB with similar RBs has already
                  //been decoded. If m_dropRbOnCollisionEnabled == true, all the collided TBs
                  //are marked corrupt and this for loop will break in the first "if" condition
                  if (collidedRbBitmap.find (*rbIt) != collidedRbBitmap.end ())
                    {
                      NS_LOG_DEBUG (*rbIt << " TB collided, labeled as corrupted!");
                      (*itTbDisc).second.corrupt = true;
                      break;
                    }
                  if (rbDecodedBitmap.find (*rbIt) != rbDecodedBitmap.end ())
                    {
                      NS_LOG_DEBUG (*rbIt << " TB with the similar RB has already been decoded. Avoid to decode it again!");
                      (*itTbDisc).second.corrupt = true;
                      break;
                    }
                }

          TbErrorStats_t tbStats = LteNistErrorModel::GetPsdchBler (m_fadingModel,LteNistErrorModel::SISO, GetMeanSinr (m_slSinrPerceived[(*itSinrDisc).second] * m_slRxGain, (*itTbDisc).second.rbBitmap),  harqInfoList);
          (*itTbDisc).second.sinr = tbStats.sinr;

          if (!((*itTbDisc).second.corrupt))
            {
              NS_LOG_DEBUG ("RB not collided");
              if (m_slHarqPhyModule->IsDiscTbPrevDecoded ((*itTbDisc).first.m_rnti, (*itTbDisc).first.m_resPsdch))
                {
                  NS_LOG_DEBUG ("TB previously decoded. Consider it not corrupted");
                  (*itTbDisc).second.corrupt = false;
                }
              else
                {
                  double rndVal = m_random->GetValue ();
                  NS_LOG_DEBUG ("TBLER is " << tbStats.tbler << " random number drawn is " << rndVal);
                  (*itTbDisc).second.corrupt = rndVal > tbStats.tbler ? false : true;
                  NS_LOG_DEBUG ("Is TB marked as corrupted after tossing the coin? " << (*itTbDisc).second.corrupt);
                }
            }

          NS_LOG_DEBUG (this << " from RNTI " << (*itTbDisc).first.m_rnti << " TBLER " << tbStats.tbler << " corrupted " << (*itTbDisc).second.corrupt <<
                        " Sinr " << (*itTbDisc).second.sinr);

          //If the TB is not corrupt and has already been decoded means that it is a retransmission.
          //We logged it to discard overlapping retransmissions.
          if (!(*itTbDisc).second.corrupt && m_slHarqPhyModule->IsDiscTbPrevDecoded ((*itTbDisc).first.m_rnti, (*itTbDisc).first.m_resPsdch))
            {
              rbDecodedBitmap.insert((*itTbDisc).second.rbBitmap.begin (), (*itTbDisc).second.rbBitmap.end ());
            }

          //If the TB is not corrupt and has not been decoded before, we indicate it decoded and consider its reception
          //successful.
          //**NOTE** If the TB is not corrupt and was previously decoded; it means that we have already reported the TB
          //reception to the PHY, therefore, we do not report it again.

          if (!(*itTbDisc).second.corrupt && !m_slHarqPhyModule->IsDiscTbPrevDecoded ((*itTbDisc).first.m_rnti, (*itTbDisc).first.m_resPsdch))
            {
              NS_LOG_DEBUG (this << " from RNTI " << (*itTbDisc).first.m_rnti << " corrupted " << (*itTbDisc).second.corrupt << " Previously decoded " <<
                            m_slHarqPhyModule->IsDiscTbPrevDecoded ((*itTbDisc).first.m_rnti, (*itTbDisc).first.m_resPsdch));
              m_slHarqPhyModule->IndicateDiscTbPrevDecoded ((*itTbDisc).first.m_rnti, (*itTbDisc).first.m_resPsdch);
              Ptr<LteControlMessage> rxCtrlMsg = m_rxPacketInfo[(*itSinrDisc).second].m_rxControlMessage;
              rxDiscMessageOkList.push_back (rxCtrlMsg);
              //Store the indices of the decoded RBs
              rbDecodedBitmap.insert((*itTbDisc).second.rbBitmap.begin (), (*itTbDisc).second.rbBitmap.end ());
            }
          //Store the HARQ information
          m_slHarqPhyModule->UpdateDiscHarqProcessStatus ((*itTbDisc).first.m_rnti, (*itTbDisc).first.m_resPsdch, (*itTbDisc).second.sinr);
        }
      else
        {
          //No error model enabled.
          //If m_dropRbOnCollisionEnabled == true, collided TBs will be marked as corrupted.
          //Else, TB will be received irrespective of collision and BLER, if it has not been decoded before.
          if (m_dropRbOnCollisionEnabled)
            {
              NS_LOG_DEBUG (this << " PSDCH DropOnCollisionEnabled: Labeling Corrupted TB");
              //Check if any of the RBs in this TB have been collided
              for (std::vector<int>::iterator rbIt =  (*itTbDisc).second.rbBitmap.begin (); rbIt != (*itTbDisc).second.rbBitmap.end (); rbIt++)
                {
                  if (collidedRbBitmap.find (*rbIt) != collidedRbBitmap.end ())
                    {
                      NS_LOG_DEBUG (*rbIt << " TB collided, labeled as corrupted!");
                      (*itTbDisc).second.corrupt = true;
                      break;
                    }
                  else
                    {
                      //We will check all the RBs and label the TB as not corrupted for every non-collided RBs.
                      //if any of the RB would collide, the above "if" will label the TB as corrupt and break.
                      NS_LOG_DEBUG (*rbIt << " RB not collided");
                      (*itTbDisc).second.corrupt = false;
                    }
                }
            }
          else
            {
              //m_dropRbOnCollisionEnabled == false. TB labeled as not corrupted. It will be received by the UE if
              //it has not been decoded before.
              (*itTbDisc).second.corrupt = false;
            }
          if (!(*itTbDisc).second.corrupt && !m_slHarqPhyModule->IsDiscTbPrevDecoded ((*itTbDisc).first.m_rnti, (*itTbDisc).first.m_resPsdch))
            {
              NS_LOG_DEBUG (this << " from RNTI " << (*itTbDisc).first.m_rnti << " corrupted " << (*itTbDisc).second.corrupt << " Previously decoded " <<
                            m_slHarqPhyModule->IsDiscTbPrevDecoded ((*itTbDisc).first.m_rnti, (*itTbDisc).first.m_resPsdch));
              m_slHarqPhyModule->IndicateDiscTbPrevDecoded ((*itTbDisc).first.m_rnti, (*itTbDisc).first.m_resPsdch);
              Ptr<LteControlMessage> rxCtrlMsg = m_rxPacketInfo[(*itSinrDisc).second].m_rxControlMessage;
              rxDiscMessageOkList.push_back (rxCtrlMsg);
            }
        }

      //traces for discovery rx
      //we would know it is discovery mcs=0 and size=232
      PhyReceptionStatParameters params;
      params.m_timestamp = Simulator::Now ().GetMilliSeconds ();
      params.m_cellId = m_cellId;
      params.m_imsi = 0;     // it will be set by DlPhyTransmissionCallback in LteHelper
      params.m_rnti = (*itTbDisc).first.m_rnti;
      params.m_txMode = m_transmissionMode;
      params.m_layer =  0;
      params.m_mcs = 0;     //for discovery, we use a fixed modulation (no mcs defined), use 0 to identify discovery
      params.m_size = 232;     // discovery message has a static size
      params.m_rv = (*itTbDisc).second.rv;
      params.m_ndi = (*itTbDisc).second.ndi;
      params.m_correctness = (uint8_t) !(*itTbDisc).second.corrupt;
      params.m_sinrPerRb = GetMeanSinr (m_slSinrPerceived[(*itSinrDisc).second] * m_slRxGain, (*itTbDisc).second.rbBitmap);
      params.m_rv = harqInfoList.size ();
      m_slPhyReception (params);
    }

  if (foundDiscoveryMsg)
    {
      if (rxDiscMessageOkList.size () > 0)
        {
          if (!m_ltePhyRxCtrlEndOkCallback.IsNull ())
            {
              NS_LOG_DEBUG (this << " Discovery OK");
              m_ltePhyRxCtrlEndOkCallback (rxDiscMessageOkList);
            }
          else
            {
              NS_ABORT_MSG ("There are correctly received Disc messages but LtePhyRxCtrlEndOkCallback is NULL");
            }
        }
      else
        {
          if (!m_ltePhyRxCtrlEndErrorCallback.IsNull ())
            {
              NS_LOG_DEBUG (this << " Discovery Error");
              m_ltePhyRxCtrlEndErrorCallback ();
            }
        }
    }
}



} // namespace ns3
