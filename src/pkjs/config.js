module.exports = [
  {
    type: 'heading',
    defaultValue: 'Nearby Wiki',
  },
  {
    type: 'text',
    defaultValue: 'Shows Wikipedia articles about places around you.',
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Wikipedia',
      },
      {
        type: 'select',
        messageKey: 'LANG',
        defaultValue: 'en',
        label: 'Language',
        options: [
          { label: 'English', value: 'en' },
          { label: 'Íslenska', value: 'is' },
          { label: 'Dansk', value: 'da' },
          { label: 'Norsk', value: 'no' },
          { label: 'Svenska', value: 'sv' },
          { label: 'Suomi', value: 'fi' },
          { label: 'Deutsch', value: 'de' },
          { label: 'Nederlands', value: 'nl' },
          { label: 'Français', value: 'fr' },
          { label: 'Español', value: 'es' },
          { label: 'Italiano', value: 'it' },
          { label: 'Português', value: 'pt' },
          { label: 'Polski', value: 'pl' },
          { label: 'Русский', value: 'ru' },
          { label: '日本語', value: 'ja' },
          { label: '中文', value: 'zh' },
        ],
      },
      {
        type: 'select',
        messageKey: 'RADIUS',
        defaultValue: '10000',
        label: 'Search radius',
        options: [
          { label: '1 km', value: '1000' },
          { label: '2 km', value: '2000' },
          { label: '5 km', value: '5000' },
          { label: '10 km', value: '10000' },
        ],
      },
      {
        type: 'input',
        messageKey: 'LANG_CUSTOM',
        defaultValue: '',
        label: 'Language code override',
        description:
          'Any Wikipedia language code, e.g. "haw". Takes precedence over ' +
          'the dropdown when set.',
        attributes: {
          placeholder: 'e.g. haw',
          limit: 8,
        },
      },
    ],
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Display',
      },
      {
        type: 'toggle',
        messageKey: 'UNITS',
        label: 'Imperial units',
        description: 'Show distances in feet and miles instead of meters.',
        defaultValue: false,
      },
    ],
  },
  {
    type: 'submit',
    defaultValue: 'Save',
  },
];
