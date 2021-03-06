/*!
*  \file    CSHModel.cpp
*  \author  Caleb Amoa Buahin <caleb.buahin@gmail.com>
*  \version 1.0.0
*  \section Description
*  This file and its associated files and libraries are free software;
*  you can redistribute it and/or modify it under the terms of the
*  Lesser GNU Lesser General Public License as published by the Free Software Foundation;
*  either version 3 of the License, or (at your option) any later version.
*  fvhmcompopnent.h its associated files is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.(see <http://www.gnu.org/licenses/> for details)
*  \date 2018
*  \pre
*  \bug
*  \todo
*  \warning
*/
#include "stdafx.h"
#include "cshmodel.h"
#include "cshcomponent.h"
#include "spatial/point.h"
#include "spatial/network.h"
#include "element.h"
#include "elementjunction.h"
#include "spatial/edge.h"
#include "iboundarycondition.h"

using namespace std;

CSHModel::CSHModel(CSHComponent *component)
  : QObject(component),
    m_timeStep(0.0001), //seconds
    m_maxTimeStep(0.5), //seconds
    m_minTimeStep(0.001), //seconds,
    m_timeStepRelaxationFactor(0.8),
    m_pressureRatio(1.0),
    m_numInitFixedTimeSteps(2),
    m_numCurrentInitFixedTimeSteps(0),
    m_printFrequency(10),
    m_currentPrintCount(0),
    m_flushToDiskFrequency(10),
    m_currentflushToDiskCount(0),
    m_computeDispersion(0.0),
    m_useAdaptiveTimeStep(true),
    m_verbose(false),
    m_useEvaporation(false),
    m_useConvection(false),
    m_advectionMode(AdvectionDiscretizationMode::Upwind),
    m_TVDFluxLimiter(ElementAdvTVD::MIN_MOD),
    m_odeSolver(nullptr),
    m_waterDensity(1000.0), //kg/m^3
    m_cp(4184.0), //4187.0 J/kg/C
    m_evapWindFuncCoeffA(1.505e-8),
    m_evapWindFuncCoeffB( 1.600e-8),
    m_bowensCoeff(0.061),
    #ifdef USE_NETCDF
    m_outputNetCDF(nullptr),
    #endif
    m_retrieveCouplingDataFunction(nullptr),
    m_component(component)
{
  m_odeSolver = new ODESolver(1, ODESolver::CVODE_ADAMS);
}

CSHModel::~CSHModel()
{

  for(Element *element : m_elements)
    delete element;

  m_elements.clear();
  m_elementsById.clear();


  for(ElementJunction *elementJunction : m_elementJunctions)
    delete elementJunction;

  m_elementJunctions.clear();
  m_elementJunctionsById.clear();

  delete m_odeSolver;

  closeOutputFiles();

  m_timeSeries.clear();

  for(IBoundaryCondition *boundaryCondition : m_boundaryConditions)
    delete boundaryCondition;

  m_boundaryConditions.clear();
}

double CSHModel::minTimeStep() const
{
  return m_minTimeStep;
}

void CSHModel::setMinTimeStep(double timeStep)
{
  m_minTimeStep = timeStep;
}

double CSHModel::maxTimeStep() const
{
  return m_maxTimeStep;
}

void CSHModel::setMaxTimeStep(double timeStep)
{
  m_maxTimeStep = timeStep;
}

bool CSHModel::useAdaptiveTimeStep() const
{
  return m_useAdaptiveTimeStep;
}

void CSHModel::setUseAdaptiveTimeStep(bool use)
{
  m_useAdaptiveTimeStep = use;
}

double CSHModel::timeStepRelaxationFactor() const
{
  return m_timeStepRelaxationFactor;
}

void CSHModel::setTimeStepRelaxationFactor(double tStepRelaxFactor)
{
  if(tStepRelaxFactor > 0)
    m_timeStepRelaxationFactor = tStepRelaxFactor;
}

double CSHModel::currentTimeStep() const
{
  return m_timeStep;
}

double CSHModel::startDateTime() const
{
  return m_startDateTime;
}

void CSHModel::setStartDateTime(double dateTime)
{
  m_startDateTime = dateTime;
}

double CSHModel::endDateTime() const
{
  return m_endDateTime;
}

void CSHModel::setEndDateTime(double dateTime)
{
  m_endDateTime = dateTime;
}

double CSHModel::outputInterval() const
{
  return m_outputInterval;
}

void CSHModel::setOutputInterval(double interval)
{
  m_outputInterval = interval;
}

double CSHModel::currentDateTime() const
{
  return m_currentDateTime;
}

ODESolver *CSHModel::odeSolver() const
{
  return m_odeSolver;
}

bool CSHModel::computeLongDispersion() const
{
  return m_computeDispersion ? true : false;
}

void CSHModel::setComputeLongDispersion(bool calculate)
{
  m_computeDispersion = calculate;

  for(Element *element : m_elements)
  {
    element->longDispersion.isBC = m_computeDispersion == false;
  }
}

CSHModel::AdvectionDiscretizationMode CSHModel::advectionDiscretizationMode() const
{
  return m_advectionMode;
}

void CSHModel::setAdvectionDiscretizationMode(AdvectionDiscretizationMode advectionDiscretizationMode)
{
  m_advectionMode = advectionDiscretizationMode;
}

double CSHModel::waterDensity() const
{
  return m_waterDensity;
}

void CSHModel::setWaterDensity(double value)
{
  m_waterDensity = value;
}

double CSHModel::specificHeatCapacityWater() const
{
  return m_cp;
}

void CSHModel::setSpecificHeatCapacityWater(double value)
{
  m_cp = value;
}

int CSHModel::numSolutes() const
{
  return m_numSolutes;
}

void CSHModel::setNumSolutes(int numSolutes)
{
  m_numSolutes = numSolutes >= 0 ? numSolutes : 0;

  if(m_simulateWaterAge)
  {
    int size = m_numSolutes + 1;

    m_solutes.resize(size);
    m_maxSolute.resize(size);
    m_minSolute.resize(size);
    m_totalSoluteMassBalance.resize(size);
    m_totalAdvDispSoluteMassBalance.resize(size);
    m_totalExternalSoluteFluxMassBalance.resize(size);
    m_solute_first_order_k.resize(size, 0.0);

    for(int i = 0 ; i < size; i++)
    {
      if(i < size - 1)
      {
        m_solutes[i] = "Solute_" + std::to_string(i + 1);
      }
      else
      {
        m_solutes[i] = "WATER_AGE";
      }
    }
  }
  else if(m_numSolutes >= 0)
  {
    int soluteNamesSize = m_solutes.size();

    m_solutes.resize(numSolutes);
    m_maxSolute.resize(numSolutes);
    m_minSolute.resize(numSolutes);
    m_totalSoluteMassBalance.resize(numSolutes);
    m_totalAdvDispSoluteMassBalance.resize(numSolutes);
    m_totalExternalSoluteFluxMassBalance.resize(numSolutes);
    m_solute_first_order_k.resize(numSolutes, 0.0);

    for(size_t i = 0 ; i < m_solutes.size(); i++)
    {
      if((int)i >= soluteNamesSize)
      {
        m_solutes[i] = "Solute_" + std::to_string(i + 1);
      }
    }
  }
}

void CSHModel::setSoluteName(int soluteIndex, const string &soluteName)
{
  m_solutes[soluteIndex] = soluteName;
}

string CSHModel::solute(int soluteIndex) const
{
  return m_solutes[soluteIndex];
}

bool CSHModel::simulateWaterAge() const
{
  return m_simulateWaterAge;
}

void CSHModel::setSimulateWaterAge(bool simulate)
{
  m_simulateWaterAge = simulate;
  setNumSolutes(m_numSolutes);
}

int CSHModel::numElementJunctions() const
{
  return m_elementJunctions.size();
}

ElementJunction *CSHModel::addElementJunction(const string &id, double x, double y, double z)
{
  if(m_elementJunctionsById.find(id) == m_elementJunctionsById.end())
  {
    ElementJunction *eJunction = new ElementJunction(id, x, y, z, this);
    eJunction->tIndex = m_elementJunctions.size();
    m_elementJunctions.push_back(eJunction);
    m_elementJunctionsById[id] = eJunction;
    return eJunction;
  }

  return nullptr;
}

void CSHModel::deleteElementJunction(const string &id)
{
  std::unordered_map<string,ElementJunction*>::iterator eJIter =  m_elementJunctionsById.find(id) ;

  if(eJIter != m_elementJunctionsById.end())
  {
    ElementJunction *eJunction = eJIter->second;
    m_elementJunctionsById.erase(eJIter);

    std::vector<ElementJunction*>::iterator it = std::find(m_elementJunctions.begin(), m_elementJunctions.end(), eJunction);
    if(it != m_elementJunctions.end())
    {
      m_elementJunctions.erase(it);
    }

    delete eJunction;
  }
}

void CSHModel::deleteElementJunction(int index)
{
  ElementJunction *eJunction = m_elementJunctions[index];

  m_elementJunctionsById.erase(eJunction->id);

  std::vector<ElementJunction*>::iterator it = std::find(m_elementJunctions.begin(), m_elementJunctions.end(), eJunction);
  if(it != m_elementJunctions.end())
    m_elementJunctions.erase(it);

  delete eJunction;
}

ElementJunction *CSHModel::getElementJunction(const string &id)
{
  return m_elementJunctionsById[id];
}

ElementJunction *CSHModel::getElementJunction(int index)
{
  return m_elementJunctions[index];
}

int CSHModel::numElements() const
{
  return m_elements.size();
}

Element *CSHModel::addElement(const string &id, ElementJunction *upStream, ElementJunction *downStream)
{
  if(upStream && downStream)
  {
    Element *element = new Element(id, upStream, downStream, this);
    element->tIndex = m_elements.size();
    m_elements.push_back(element);
    m_elementsById[id] = element;
    return element;
  }

  return nullptr;
}

void CSHModel::deleteElement(const string &id)
{
  unordered_map<string,Element*>::iterator eIter = m_elementsById.find(id);

  if(eIter != m_elementsById.end())
  {
    Element *element = eIter->second;
    m_elementsById.erase(eIter);

    vector<Element*>::iterator it = std::find(m_elements.begin() , m_elements.end(), element);
    if(it != m_elements.end())
      m_elements.erase(it);

    delete element;
  }
}

void CSHModel::deleteElement(int index)
{
  Element *element = m_elements[index];
  m_elementJunctionsById.erase(element->id);

  vector<Element*>::iterator it = std::find(m_elements.begin() , m_elements.end(), element);

  if(it != m_elements.end())
    m_elements.erase(it);

  delete element;
}

Element *CSHModel::getElement(const string &id)
{
  return m_elementsById[id];
}

Element *CSHModel::getElement(int index)
{
  return m_elements[index];
}

RetrieveCouplingData CSHModel::retrieveCouplingDataFunction() const
{
  return m_retrieveCouplingDataFunction;
}

void CSHModel::setRetrieveCouplingDataFunction(RetrieveCouplingData retrieveCouplingDataFunction)
{
  m_retrieveCouplingDataFunction = retrieveCouplingDataFunction;
}

bool CSHModel::initialize(list<string> &errors)
{
  bool initialized = initializeInputFiles(errors) &&
                     initializeTimeVariables(errors) &&
                     initializeElements(errors) &&
                     initializeSolver(errors) &&
                     initializeOutputFiles(errors) &&
                     initializeBoundaryConditions(errors);



  if(initialized)
  {


    applyInitialConditions();

  }

  return initialized;
}

bool CSHModel::finalize(std::list<string> &errors)
{
  closeOutputFiles();

  for(IBoundaryCondition *boundaryCondition : m_boundaryConditions)
    delete boundaryCondition;

  m_boundaryConditions.clear();

  return true;
}

bool CSHModel::initializeTimeVariables(std::list<string> &errors)
{
  if(m_startDateTime >= m_endDateTime)
  {
    errors.push_back("End datetime must be greater than startdatetime");
    return false;
  }

  if( (m_endDateTime - m_startDateTime) *  86400.0 < m_minTimeStep )
  {
    errors.push_back("Make sure timestep is less than the simulation interval");
    return false;
  }

  if(m_minTimeStep <=  0 || m_maxTimeStep <= 0)
  {
    errors.push_back("Make sure time steps are greater 0");
    return false;
  }

  if(m_minTimeStep > m_maxTimeStep)
  {
    errors.push_back("");
    return false;
  }

  m_numCurrentInitFixedTimeSteps = 0;

  m_currentDateTime = m_startDateTime;
  m_nextOutputTime = m_currentDateTime;

  m_currentPrintCount = 0;
  m_currentflushToDiskCount = 0;

  return true;
}

bool CSHModel::initializeElements(std::list<string> &errors)
{

#ifdef USE_OPENMP
#pragma omp parallel for
#endif
  for(int i = 0 ; i < (int)m_elementJunctions.size()  ; i++)
  {
    ElementJunction *elementJunction = m_elementJunctions[i];
    size_t numElements = elementJunction->incomingElements.size() + elementJunction->outgoingElements.size();
    elementJunction->tIndex = -1;

    switch (numElements)
    {
      case 0:
        elementJunction->junctionType = ElementJunction::NoElement;
        break;
      case 1:
        elementJunction->junctionType = ElementJunction::SingleElement;
        break;
      case 2:
        elementJunction->junctionType = ElementJunction::DoubleElement;
        break;
      default:
        elementJunction->junctionType = ElementJunction::MultiElement;
        break;
    }

  }

  m_solverSize = 0;

  if(m_solveHydraulics)
  {
    for(int i = 0 ; i < (int)m_elements.size()  ; i++)
    {
      Element *element = m_elements[i];
      element->hIndex = m_solverSize; m_solverSize++;
      element->distanceFromUpStreamJunction = 0;
    }
  }
  else
  {
    for(int i = 0 ; i < (int)m_elements.size()  ; i++)
    {
      Element *element = m_elements[i];
      element->hIndex = -1;
    }
  }


  for(int i = 0 ; i < (int)m_elements.size()  ; i++)
  {
    Element *element = m_elements[i];
    element->tIndex = m_solverSize; m_solverSize++;
    element->initialize();
  }


  for(size_t i = 0 ; i < m_elementJunctions.size()  ; i++)
  {
    ElementJunction *elementJunction = m_elementJunctions[i];

    if(elementJunction->junctionType == ElementJunction::MultiElement)
    {
      if(!elementJunction->temperature.isBC)
      {
        elementJunction->tIndex = m_solverSize; m_solverSize++;
      }
      else
      {
        elementJunction->tIndex = -1;
      }
    }
  }

  for(size_t j = 0 ; j < m_solutes.size(); j++)
  {
    for(int i = 0 ; i < (int)m_elements.size()  ; i++)
    {
      Element *element = m_elements[i];
      element->sIndex[j] = m_solverSize; m_solverSize++;
    }

    for(size_t i = 0 ; i < m_elementJunctions.size()  ; i++)
    {
      ElementJunction *elementJunction = m_elementJunctions[i];

      if(elementJunction->junctionType == ElementJunction::MultiElement)
      {
        if(!elementJunction->soluteConcs[j].isBC)
        {
          //If more than one junction solve continuity
          elementJunction->sIndex[j] = m_solverSize; m_solverSize++;;
        }
        else
        {
          elementJunction->sIndex[j] = -1;
        }
      }
    }
  }

  m_eligibleJunctions.clear();

  for(size_t i = 0 ; i < m_elementJunctions.size()  ; i++)
  {
    ElementJunction *elementJunction = m_elementJunctions[i];

    if(elementJunction->tIndex > -1)
    {
      m_eligibleJunctions.push_back(elementJunction);
    }
    else
    {
      for(size_t j = 0; j < m_solutes.size(); j++)
      {
        if(elementJunction->sIndex[j] > -1)
        {
          m_eligibleJunctions.push_back(elementJunction);
          break;
        }
      }
    }
  }

  for(int i = 0 ; i < (int)m_elements.size()  ; i++)
  {
    calculateDistanceFromUpstreamJunction(m_elements[i]);
  }


  return true;
}

bool CSHModel::initializeSolver(std::list<string> &errors)
{
  //  int totalCells = (int) m_elements.size();

  //  m_soluteIndexes.clear();

  //  for(size_t i = 0 ; i < m_solutes.size(); i++)
  //  {
  //    m_soluteIndexes.push_back(totalCells);
  //    totalCells += m_elements.size();
  //  }

  m_solverCurrentValues.resize(m_solverSize, 0.0);
  m_solverOutputValues.resize(m_solverSize, 0.0);

  m_odeSolver->setSize(m_solverSize);
  m_odeSolver->initialize();

  return true;
}

bool CSHModel::initializeBoundaryConditions(std::list<string> &errors)
{
  for(size_t i = 0; i < m_boundaryConditions.size() ; i++)
  {
    IBoundaryCondition *boundaryCondition = m_boundaryConditions[i];
    boundaryCondition->clear();
    boundaryCondition->findAssociatedGeometries();
    boundaryCondition->prepare();
  }

  return true;
}

bool CSHModel::findProfile(Element *from, Element *to, std::vector<Element *> &profile)
{
  if(from == to)
  {
    profile.push_back(from);
    return true;
  }
  else
  {
    for(Element *outgoing : from->downstreamJunction->outgoingElements)
    {
      if(outgoing == to)
      {
        profile.push_back(from);
        profile.push_back(outgoing);
        return true;
      }
      else if(findProfile(outgoing, to, profile))
      {
        profile.insert(profile.begin(), from);
        return true;
      }
    }
  }

  return false;
}

void CSHModel::calculateDistanceFromUpstreamJunction(Element *element)
{
  if(element->distanceFromUpStreamJunction == 0)
  {
    if(element->upstreamElement != nullptr)
    {
      if(element->upstreamElement->distanceFromUpStreamJunction == 0)
      {
        calculateDistanceFromUpstreamJunction(element->upstreamElement);
      }

      element->distanceFromUpStreamJunction = element->upstreamElement->distanceFromUpStreamJunction + element->length / 2.0;
    }
    else
    {
      element->distanceFromUpStreamJunction = element->length / 2.0;
    }
  }
}
