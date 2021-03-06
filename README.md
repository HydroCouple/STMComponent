# CSHComponent
[![AUR](https://img.shields.io/badge/license-GPL-blue.svg)](https://github.com/HydroCouple/CSHComponent/blob/master/LICENSE)

A one-dimensional stream heat and solute transport model.

## Input File Specification
---------------------------------------

The CSHComponent can be initialized from an input file described in the documents or initialized programmatically as shown in the following code.

## Sample Code
---------------------------------------
``` C++


    /*!
     * Case 2 Page 137 (Versteeg, H.K. and W. Malalasekera, 2007.
     * An Introduction to Computational Fluid Dynamics: The Finite Volume Method. Pearson Education Ltd., Harlow, England; New York.)
     * in using the upwind differencing method for advection discretization.
     */

    //Error Messages
    std::list<std::string> errors;

    //Stream temperature model instance
    CSHModel *model = new CSHModel(nullptr);

    //Set advection discretization mode
    model->setAdvectionDiscretizationMode(CSHModel::AdvectionDiscretizationMode::Upwind);

    //set time and output variables
    model->setMaxTimeStep(0.5); //seconds
    model->setMinTimeStep(0.0001); //seconds
    model->setStartDateTime(0); // Modified Julian DateTime
    model->setEndDateTime(1.0 / 1440.0);// Modified Julian DateTime 1 minute
    model->setOutputInterval(1.0); //1 second

    //Output
    model->setOutputCSVFile(QFileInfo("../../examples/Versteeg/case2/versteegcase2_upwind.csv"));

    //Set solver type
    model->solver()->setSolverType(ODESolver::CVODE_ADAMS);

    //  Domain spatial discretization
    double x , y = 0, z = 0;
    int numCells = 5.0;
    double dx = 1.0 / numCells;

    //Channel Junctions connections
    for(int i = 0; i <= numCells; i++)
    {
      //Make it explicit what each variable means
      x = i * dx;
      ElementJunction *eJunction = model->addElementJunction(QString::number(i).toStdString(), x, y, z);

      if(i == 0)
      {
        eJunction->temperature.isBC = true;
        eJunction->temperature.value = 1.0;
      }
      else if(i == numCells)
      {
        eJunction->temperature.isBC = true;
        eJunction->temperature.value = 0.0;
      }
    }

    //Channels
    for(int i = 0; i < numCells ; i++)
    {
      Element *element = model->addElement(QString::number(i).toStdString(), model->getElementJunction(i), model->getElementJunction(i+1));
      element->xSectionArea = 0.1;
      element->length = dx;
      element->flow = element->xSectionArea * 2.5;
      element->depth = 0.1;
      element->longDispersion.isBC = true;
      element->longDispersion.value = 0.1;
    }

    //initialize model
    if(model->initialize(errors))
    {
      //Perform timestep until completion
      while (model->currentDateTime() < model->endDateTime())
      {
        model->update();
      }
    }

    //finalize model
    model->finalize(errors);
    delete model;

```

## Publications
---------------------------------------

* Buahin, C. A., Horsburgh, J. S. and Neilson, B. T.: Parallel multi-objective calibration of a component-based river temperature model, Environmental Modelling & Software, 116, 57–71, doi:10.1016/j.envsoft.2019.02.012, 2019.
