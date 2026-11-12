// Hook table of layer_view.cpp (included into its ZAX_HOOKS list): address, replacement, name, length of the
// overwritten instructions, the first 8 bytes of the original and the offset + 1 of a relative call
// among them; lengths and bytes read from Zax.exe build 1308 (SHA-1 a918c784) with capstone.
        {0x4ec280, ptr(&V::MiniMapImageRect), "Map_MiniMapImageRect", 5, {0x83, 0xec, 0x14, 0x53, 0x55, 0x56, 0x57, 0x8b}},
        {0x4ec3c0, ptr(&V::WorldRectToMiniMap), "Map_WorldRectToMiniMap", 5, {0x83, 0xec, 0x28, 0x53, 0x56, 0x8b, 0xf1, 0x57}},
        {0x4ed6e0, ptr(&V::ScrollToward), "CLayer::ScrollViewToward", 5, {0x51, 0xd9, 0x44, 0x24, 0x1c, 0xd8, 0x0d, 0x2c}},
        {0x4ed7f0, ptr(&V::ClampView), "CLayer::ClampView", 7, {0x83, 0xec, 0x20, 0x56, 0x8b, 0x71, 0x7c, 0x57}},
        {0x4ed8f0, ptr(&V::SetView), "CLayer::SetView", 6, {0x55, 0x8b, 0xec, 0x56, 0x8b, 0xf1, 0x8b, 0x46}},
