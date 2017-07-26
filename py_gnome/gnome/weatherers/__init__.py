from core import Weatherer, HalfLifeWeatherer
from cleanup import Skimmer, Burn, ChemicalDispersion
from manual_beaching import Beaching
from evaporation import Evaporation
from natural_dispersion import NaturalDispersion
from dissolution import Dissolution
from emulsification import Emulsification
from weathering_data import WeatheringData
from spreading import Langmuir, FayGravityViscous, ConstantArea
from roc import Burn as ROC_Burn
from roc import Disperse as ROC_Disperse

'''
    Weatherers are to be ordered as follows:

    half-life - This weatherer is just used for testing.  It is not used
        with the following real weatherers but we need to include it so
        sorting always works

    Cleanup options including Skimmer, Burn, Beaching

    WeatheringData - defines initialize_data() method to initialize all
        weathering data arrays. In weather_elements, this updates data arrays
        corresponding with wd properties (density, viscosity)

    FayGravityViscous - initializes the 'fay_area' and 'area' array. Also
        updates the 'area' and 'fay_area' array

    Langmuir - modifies the 'area' array with fractional coverage based on
        langmuir cells.

    Removal options have been re-prioritized - Burn, Skim, Disperse, Beach
        the first three are listed in reverse order because the marking done
        in prepare_for_model_step prioritizes whichever operation gets marked
        last.
        Once they are marked the weathering order doesn't matter.
'''

# NOTE: this list specifies sort order!
sort_order = [ChemicalDispersion,
              Skimmer,
              Burn,
              ROC_Burn,
              ROC_Disperse,
              Beaching,
              HalfLifeWeatherer,
              Evaporation,
              NaturalDispersion,
              # OilParticleAggregation,
              Dissolution,
              # Biodegradation,
              Emulsification,
              WeatheringData,
              FayGravityViscous,
              ConstantArea,
              Langmuir,
              ]

weatherers_idx = dict([(v, i) for i, v in enumerate(sort_order)])


def weatherer_sort(weatherer):
    '''
        Returns an int describing the sorting order of the weatherer
        or None if an order is not defined for the weatherer

        :param weatherer: weatherer instance
    '''
    return weatherers_idx.get(weatherer.__class__)
