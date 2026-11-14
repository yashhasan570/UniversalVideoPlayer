osxbundle = custom_target('osxbundle',
    input: join_paths('universalvideoplayer.app', 'Contents', 'Resources', 'icon.icns'),
    output: 'icon.icns.inc',
    command: [file2string, '@INPUT@', '@OUTPUT@', '@SOURCE_ROOT@'],
)
sources += osxbundle
