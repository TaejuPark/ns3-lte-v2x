/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * NIST-developed software is provided by NIST as a public
 * service. You may use, copy and distribute copies of the software in
 * any medium, provided that you keep intact this entire notice. You
 * may improve, modify and create derivative works of the software or
 * any portion of the software, and you may copy and distribute such
 * modifications or works. Modified works should carry a notice
 * stating that you changed the software and should note the date and
 * nature of any such change. Please explicitly acknowledge the
 * National Institute of Standards and Technology as the source of the
 * software.
 *
 * NIST-developed software is expressly provided "AS IS." NIST MAKES
 * NO WARRANTY OF ANY KIND, EXPRESS, IMPLIED, IN FACT OR ARISING BY
 * OPERATION OF LAW, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTY OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE,
 * NON-INFRINGEMENT AND DATA ACCURACY. NIST NEITHER REPRESENTS NOR
 * WARRANTS THAT THE OPERATION OF THE SOFTWARE WILL BE UNINTERRUPTED
 * OR ERROR-FREE, OR THAT ANY DEFECTS WILL BE CORRECTED. NIST DOES NOT
 * WARRANT OR MAKE ANY REPRESENTATIONS REGARDING THE USE OF THE
 * SOFTWARE OR THE RESULTS THEREOF, INCLUDING BUT NOT LIMITED TO THE
 * CORRECTNESS, ACCURACY, RELIABILITY, OR USEFULNESS OF THE SOFTWARE.
 *
 * You are solely responsible for determining the appropriateness of
 * using and distributing the software and you assume all risks
 * associated with its use, including but not limited to the risks and
 * costs of program errors, compliance with applicable laws, damage to
 * or loss of data, programs or equipment, and the unavailability or
 * interruption of operation. This software is not intended to be used
 * in any situation where a failure could cause risk of injury or
 * damage to property. The software developed by NIST employees is not
 * subject to copyright protection within the United States.
 */

#include "ns3/log.h"
#include "ns3/double.h"
#include "ns3/enum.h"
#include "ns3/mobility-model.h"
#include <cmath>
#include <random>
#include "outdoor-to-outdoor-propagation-loss-model.h"
#include <ns3/node.h>

NS_LOG_COMPONENT_DEFINE ("OutdoorToOutdoorPropagationLossModel");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (OutdoorToOutdoorPropagationLossModel);


TypeId
OutdoorToOutdoorPropagationLossModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::OutdoorToOutdoorPropagationLossModel")

    .SetParent<PropagationLossModel> ()
    .SetGroupName ("Buildings")

    .AddConstructor<OutdoorToOutdoorPropagationLossModel> ()
    .AddAttribute ("Frequency",
                   "The propagation frequency in Hz",
                   DoubleValue (2106e6),
                   MakeDoubleAccessor (&OutdoorToOutdoorPropagationLossModel::m_frequency),
                   MakeDoubleChecker<double> ())
  ;

  return tid;
}

OutdoorToOutdoorPropagationLossModel::OutdoorToOutdoorPropagationLossModel ()
  : PropagationLossModel ()
{
  NS_LOG_FUNCTION (this);
  m_rand = CreateObject<UniformRandomVariable> ();
}

OutdoorToOutdoorPropagationLossModel::~OutdoorToOutdoorPropagationLossModel ()
{
}

double
OutdoorToOutdoorPropagationLossModel::GetLoss (Ptr<MobilityModel> a, Ptr<MobilityModel> b) const
{
  NS_LOG_FUNCTION (this);
  srand(time(NULL));
  // Free space pathloss
  double loss = 0.0;
  // Frequency in GHz
  //double fc = m_frequency / 1e9;
  double fc = 5.9;
  // Distance between the two nodes in meter
  double dist = a->GetDistanceFrom (b);

  // Calculate the pathloss based on 3GPP specifications : 3GPP TR 36.843 V12.0.1
  // WINNER II Channel Models, D1.1.2 V1.2., Equation (4.24) p.43, available at
  // http://www.cept.org/files/1050/documents/winner2%20-%20final%20report.pdf
  loss = 20 * std::log10 (dist) + 46.6 + 20 * std::log10 (fc / 5.0);
  NS_LOG_INFO (this << "Outdoor , the free space loss = " << loss);

  // WINNER II channel model for Urban Microcell scenario (UMi) : B1
  double pl_b1 = 0.0;
  // Actual antenna heights (1.5 m for UEs)
  //double hms = a->GetPosition ().z;
  //double hbs = b->GetPosition ().z;
  double hms = 1.5;
  double hbs = 1.5;
  // Effective antenna heights (0.8 m for UEs)
  double hbs1 = hbs - 1;
  double hms1 = hms - 0.7;
  // Propagation velocity in free space
  double c = 3 * std::pow (10, 8);
  // LOS offset = LOS loss to add to the computed pathloss
  double los = 0;
  double los_shadow = GetShadowing (3.0, 0);
  // NLOS offset = NLOS loss to add to the computed pathloss
  double nlos = -5;
  double nlos_shadow = GetShadowing (4.0, 0);

  double d1 = 4 * hbs1 * hms1 * m_frequency * (1 / c);

  // Calculate the LOS probability based on 3GPP specifications : 3GPP TR 36.843 V12.0.1
  // WINNER II Channel Models, D1.1.2 V1.2., Table 4-7 p.48, available at
  // http://www.cept.org/files/1050/documents/winner2%20-%20final%20report.pdf
  double plos = std::min ((18 / dist), 1.0) * (1 - std::exp (-dist / 36)) + std::exp (-dist / 36);

  // Compute the WINNER II B1 pathloss based on 3GPP specifications : 3GPP TR 36.843 V12.0.1
  // D5.3: WINNER+ Final Channel Models, Table 4-1 p.74, available at
  // http://projects.celtic-initiative.org/winner%2B/WINNER+%20Deliverables/D5.3_v1.0.pdf

  // Generate a random number between 0 and 1 (if it doesn't already exist) to evaluate the LOS/NLOS situation
  double r = 0.0;

  MobilityDuo couple;
  couple.a = a;
  couple.b = b;
  std::map<MobilityDuo, double>::iterator it_a = m_randomMap.find (couple);
  if (it_a != m_randomMap.end ())
    {
      r = it_a->second;
    }
  else
    {
      couple.a = b;
      couple.b = a;
      std::map<MobilityDuo, double>::iterator it_b = m_randomMap.find (couple);
      if (it_b != m_randomMap.end ())
        {
          r = it_b->second;
        }
      else
        {
          m_randomMap[couple] = m_rand->GetValue (0,1);
          r = m_randomMap[couple];
        }
    }

  //NS_LOG_DEBUG ("frequency carrier: " << fc);
  // This model is only valid to a minimum distance of 3 meters
  plos = r + 1.0;
  if (dist >= 3)
    {
      if (r <= plos)
        {
          // LOS
          if (dist <= d1)
            {
              pl_b1 = 22.7 * std::log10 (dist) + 41.0 + 20.0 * std::log10 (fc) + los + los_shadow;
              NS_LOG_INFO ("Outdoor LOS (Distance = " << dist << "), shadow = " << los_shadow <<", WINNER B1 loss = " << pl_b1);
            }
          else
            {
              pl_b1 = 40 * std::log10 (dist) + 9.45 - 17.3 * std::log10 (hbs1) - 17.3 * std::log10 (hms1) + 2.7 * std::log10 (fc) + los + los_shadow;
              NS_LOG_INFO ("Outdoor LOS (Distance = " << dist << "), shadow = " << los_shadow <<", WINNER B1 loss = " << pl_b1);
            }
        }
      else
        {
          // NLOS
          if ((fc >= 0.758)and (fc <= 0.798))
            {
              // Frequency = 700 MHz for Public Safety
              pl_b1 = (44.9 - 6.55 * std::log10 (hbs)) * std::log10 (dist) + 5.83 * std::log10 (hbs) + 16.33 + 26.16 * std::log10 (fc) + nlos + nlos_shadow;
              NS_LOG_INFO ("Outdoor NLOS (Distance = " << dist << "), shadow = " << nlos_shadow <<", WINNER B1 loss = " << pl_b1);
            }
          if ((fc >= 1.92)and (fc <= 2.17))
            {
              // Frequency = 2 GHz for General Scenario
              pl_b1 = (44.9 - 6.55 * std::log10 (hbs)) * std::log10 (dist) + 5.83 * std::log10 (hbs) + 14.78 + 34.97 * std::log10 (fc) + nlos + nlos_shadow;
              NS_LOG_INFO ("Outdoor NLOS (Distance = " << dist << "), shadow = " << nlos_shadow <<", WINNER B1 loss = " << pl_b1);
            }
        }
    }

  loss = std::max (loss, pl_b1);
  return std::max (0.0, loss);
}

double
OutdoorToOutdoorPropagationLossModel::GetShadowing(double stddev, int type) const
{
  std::random_device dev;
  std::mt19937 generator(dev());
  double number;
  if (type == 0) // LOS
    {
      std::lognormal_distribution <double> distribution (0.0, 3.0);
      number = distribution (generator);
    }
  else
    {
      std::lognormal_distribution <double> distribution (0.0, 4.0);
      number = distribution (generator);
    }

  return number;
}

double
OutdoorToOutdoorPropagationLossModel::DoCalcRxPower (double txPowerDbm,
                                               Ptr<MobilityModel> a,
                                               Ptr<MobilityModel> b) const
{
  NS_LOG_FUNCTION (this);
  return (txPowerDbm - GetLoss (a, b));
}

int64_t
OutdoorToOutdoorPropagationLossModel::DoAssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_rand->SetStream (stream);
  return 1;
}


} // namespace ns3
