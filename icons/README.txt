SUM Chain Ledger App Icons
===========================

Icon: Greek letter Koppa (U+03D8) on purple gradient (#26022e to #3d0847)

Required icon files for Ledger devices:

1. nanos_app_sumchain.gif
   - Size: 16x16 pixels
   - Format: 1-bit (black and white)
   - For: Nano S (legacy)

2. nanox_app_sumchain.gif
   - Size: 14x14 pixels
   - Format: 1-bit (black and white)
   - For: Nano S+, Nano X

3. stax_app_sumchain.gif
   - Size: 32x32 pixels
   - Format: 4-bit grayscale or color
   - For: Ledger Stax

4. flex_app_sumchain.gif
   - Size: 40x40 pixels
   - Format: 4-bit grayscale or color
   - For: Ledger Flex

Generation Instructions:
------------------------

1. Create a 64x64 or larger source image with the Koppa symbol
2. Use Ledger's icon tool or GIMP to convert:

   For Nano S/S+/X (1-bit):
   - Convert to grayscale
   - Apply threshold to make pure black/white
   - Resize to target dimensions
   - Export as indexed GIF with 2 colors

3. Place generated files in this directory

Alternative: Use ledger-app-builder Docker image which includes icon tools:
   docker run --rm -v $(pwd):/app ledger-app-builder
