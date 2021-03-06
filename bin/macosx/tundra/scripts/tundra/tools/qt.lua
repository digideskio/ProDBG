-- Qt-specific environment additions

module(..., package.seeall)

function apply(env, options)
  env:set_many {
    ["QTMOC"] = "moc",
    ["QTMOCOPTS"] = "",
    ["QTMOCCMD"] = "$(QTMOC) -o$(@) $(<)",
    ["QTRCC"] = "rcc",
    ["QTRCCOPTS"] = "",
    ["QTRCCCMD"] = "$(QTRCC) $(QTRCCOPTS) -o $(@) $(<)",
    ["QTUIC"] = "uic",
    ["QTUICOPTS"] = "",
    ["QTUICCMD"] = "$(QTUIC) $(QTUICOPTS) -o $(@) $(<)",
  }
end

