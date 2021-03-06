/*!
 * \file elementinput.h
 * \author  Caleb Amoa Buahin <caleb.buahin@gmail.com>
 * \version 1.0.0
 * \description
 * \license
 * This file and its associated files, and libraries are free software.
 * You can redistribute it and/or modify it under the terms of the
 * Lesser GNU Lesser General Public License as published by the Free Software Foundation;
 * either version 3 of the License, or (at your option) any later version.
 * This file and its associated files is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.(see <http://www.gnu.org/licenses/> for details)
 * \copyright Copyright 2014-2018, Caleb Buahin, All rights reserved.
 * \date 2014-2018
 * \pre
 * \bug
 * \warning
 * \todo
 */

#ifndef ELEMENTINPUT_H
#define ELEMENTINPUT_H

#include "cshcomponent_global.h"
#include "spatiotemporal/timegeometryinput.h"
#include "spatiotemporal/timegeometrymultiinput.h"

#include <unordered_map>

class CSHComponent;


class CSHCOMPONENT_EXPORT ElementInput : public TimeGeometryInputDouble
{
    Q_OBJECT

  public:

    enum VariableType
    {
      Flow,
      XSectionArea,
      STSXSectionArea,
      Depth,
      TopWidth,
      STSWidthFraction,
      DVolumeDTime
    };

    ElementInput(const QString &id,
                 Dimension *timeDimension,
                 Dimension *geometryDimension,
                 ValueDefinition *valueDefinition,
                 VariableType varType,
                 CSHComponent *modelComponent);

    virtual ~ElementInput() override;

    /*!
     * \brief setProvider
     * \param provider
     */
    bool setProvider(HydroCouple::IOutput *provider) override;

    /*!
     * \brief canConsume
     * \param provider
     * \param message
     * \return
     */
    bool canConsume(HydroCouple::IOutput *provider, QString &message) const override;

    /*!
     * \brief retrieveValuesFromProvider
     */
    void retrieveValuesFromProvider() override;

    /*!
     * \brief applyData
     */
    void applyData() override;

    /*!
     * \brief variableType
     * \return
     */
    VariableType variableType() const;

    /*!
     * \brief setVariableType
     * \param variableType
     */
    void setVariableType(VariableType variableType);

  private:

    std::unordered_map<int,int> m_geometryMapping;
    std::unordered_map<int,double> m_geometryMappingOrientation;
    CSHComponent *m_component;
    VariableType m_varType;

};

class CSHCOMPONENT_EXPORT ElementSourceInput : public  TimeGeometryMultiInputDouble
{
  public:

    enum SourceType
    {
      RadiativeFlux,
      HeatFlux,
      SoluteFlux
    };

    ElementSourceInput(const QString &id,
                           Dimension *timeDimension,
                           Dimension *geometryDimension,
                           ValueDefinition *valueDefinition,
                           SourceType srcType,
                           CSHComponent *modelComponent);

    virtual ~ElementSourceInput() override;

    bool addProvider(HydroCouple::IOutput *provider) override;

    bool removeProvider(HydroCouple::IOutput *provider) override;

    bool canConsume(HydroCouple::IOutput *provider, QString &message) const override;

    void retrieveValuesFromProvider() override;

    void applyData() override;

    SourceType sourceType() const;

    void setSourceType(SourceType srcType);

    int soluteIndex() const;

    void setSoluteIndex(int soluteIndex);

  private:

    CSHComponent *m_component;
    SourceType m_srcType;
    std::unordered_map<HydroCouple::IOutput*, std::unordered_map<int,int>> m_geometryMapping;
    int m_soluteIndex;
};

#endif // ELEMENTINPUT_H
