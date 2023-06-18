def module_to_die(module: str):
  # Returns the die revision that the module has
  if module in ["sasa00", "sasa01"]:
    return "8Gb B-Die"
  if module in ["sasa02"]:
    return "8Gb C-Die"
  if module in ["sasa04", "sasa05", "sasa0a", "sasa09"]:
    return "8Gb D-Die"
  if module in ["sags03", "sags00"]:
    return "4Gb F-Die"

  if module in ["hyhy00", "hyhy01"]:
    return "16Gb A-Die"
  if module in ["hyhy04", "hyhy05"]:
    return "16Gb C-Die"
  if module in ["hyki04"]:
    return "4Gb A-Die"
  if module in ["hyco00"]:
    return "4Gb X-Die"

  if module in ["mimi01"]:
    return "8Gb B-Die"
  if module in ["mimifc", "mimifd"]:
    return "16Gb B-Die"
  if module in ["mimiff", "mimife", "mimi40"]:
    return "16Gb E-Die"
  if module in ["mimifb"]:
    return "16Gb F-Die"

  if module in ["nyki00", "nyki01"]:
    return "8Gb C-Die"

  return "!!UNKNOWN MODULE!!"


def module_to_mfr(module: str) -> str:
  # Returns the manufacturer of the module
  if module.startswith("sa"):
    return "Samsung"
  if module.startswith("hy"):
    return "Hynix"
  if module.startswith("mi"):
    return "Micron"
  if module.startswith("ny"):
    return "Nanya"

  return "!!UNKNOWN MFR!!"


def module_to_annon_mfr(module: str) -> str:
  # Returns the annonymous manufacturer of the module
  if module.startswith("sa"):
    return "Mfr. S"
  if module.startswith("hy"):
    return "Mfr. H"
  if module.startswith("mi"):
    return "Mfr. M"
  if module.startswith("ny"):
    return "Mfr. N"

  return "!!UNKNOWN MFR!!"

mfr_order = ["Mfr. S", "Mfr. H", "Mfr. M"]

hue_orders = {
  "Mfr. S" : ["4Gb F-Die", "8Gb B-Die", "8Gb C-Die", "8Gb D-Die"],
  "Mfr. H" : ["4Gb A-Die", "4Gb X-Die", "16Gb A-Die", "16Gb C-Die"],
  "Mfr. M" : ["8Gb B-Die", "16Gb B-Die", "16Gb E-Die", "16Gb F-Die"],
}

hue_orders_coverage = {
  "Mfr. S" : ["4Gb F-Die", "8Gb B-Die", "8Gb C-Die", "8Gb D-Die"],
  "Mfr. H" : ["4Gb A-Die", "4Gb X-Die", "16Gb A-Die-1", "16Gb A-Die-2", "16Gb C-Die"],
  "Mfr. M" : ["8Gb B-Die", "16Gb B-Die", "16Gb E-Die", "16Gb F-Die"],
}


def ac_to_attack_time_ns(ras_scale: int, ac: int) -> int:
  # Convert the hcfirst to attack time
  return (9 + (ras_scale) * 5) * 6 * ac


def adjust_box_widths(g, fac):
  """
  Adjust the withs of a seaborn-generated boxplot.
  From https://stackoverflow.com/a/56955897
  """
  from matplotlib.patches import PathPatch
  import numpy as np

  # iterating through Axes instances
  for ax in g.axes:
    # iterating through axes artists:
    for c in ax.get_children():
      # searching for PathPatches
      if isinstance(c, PathPatch):
        # getting current width of box:
        p = c.get_path()
        verts = p.vertices
        verts_sub = verts[:-1]
        xmin = np.min(verts_sub[:, 0])
        xmax = np.max(verts_sub[:, 0])
        xmid = 0.5*(xmin+xmax)
        xhalf = 0.5*(xmax - xmin)
        # setting new width of box
        xmin_new = xmid-fac*xhalf
        xmax_new = xmid+fac*xhalf
        verts_sub[verts_sub[:, 0] == xmin, 0] = xmin_new
        verts_sub[verts_sub[:, 0] == xmax, 0] = xmax_new
        # setting new width of median line
        for l in ax.lines:
          if np.all(l.get_xdata() == [xmin, xmax]):
            l.set_xdata([xmin_new, xmax_new])