class App:
  def __init__(self, name, params=None, bitwidths=None, cached_construction=False, ignore_overflow=False):
    self.name = name

    #if (params==None): params = [1,2,3]
    if (params==None): params = [0]
    #if (params==None): params = [1,2,3,4,5]
    self.params = params

    if (bitwidths==None): bitwidths = [32]
    #if (bitwidths==None): bitwidths = [32, 110]
    self.bitwidths = bitwidths
    self.cached_construction = cached_construction 
    self.ignore_overflow = ignore_overflow

  @classmethod
  def defaultApps(cls):
    apps = [#App("factorial"),
      #App("two-matrix", ignore_overflow=True),
      #App("one-matrix", ignore_overflow=True),
      App("eqtest")
      #App("polynomial", ignore_overflow=True),
      #App("image-kernel"),
      #App("floyd"),
      #App("lgca", bitwidths=[32]),
      #App("sha", params=[0], bitwidths=[32], cached_construction=True),
      #App("sha", params=[1], bitwidths=[32], cached_construction=True),
      ]
    return apps

  @classmethod
  def lookupAppSettings(cls, name):
    apps = cls.defaultApps()
    for app in apps:
      if app.name == name:
        return app
    return None

