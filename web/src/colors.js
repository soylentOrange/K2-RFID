// Official colors of the Creality filaments ("hex:name"), taken from the material database
// of a K2 Plus (version 1794645521). Used when the database on the thingy doesn't list colors.
const OFFICIAL = {
  '01001': 'C12E1F:Red,0A2989:Blue,FFFFFF:White,000000:Black,B68601:Gold,9E007E:Purple,BB2649:Viva Magenta,FFC197:Peach Fuzz,6667AB:Very Peri,E7CEB5:Apricot,9D432C:Brown,9EA2A2:Grey,FF6A14:Orange,4C8D2B:Green,FCE300:Yellow,F3BBCA:Orchid Powder,FED877:Goldfish Yellow,9E91C3:Violet Tulip,006B54:Mountain Green,7CE0D3:Light Blue Green,82D8CE:Cloud Valley,0032A0:Azure,713F2A:Sandalwood,465C3E:Forest Green,892B2B:Burgundy Red,DA291C:Flame Red,E35205:Sunset Orange,FA4616:Begonia Crimson,FEDB00:Lemon Yellow,0E4174:Starlit Indigo', // Hyper PLA
  '02001': '000000:Black,212721:Dark Green,74654F:Greyish Yellow,343646:Purple,503E2D:Ochre,711712:Maroon,314B4F:Green,80523D:Brown,253B5C:Blue,A03552:Burgundy,B5984C:Yellow,9E5B32:Orange', // Hyper PLA-CF
  '06002': 'FFFFFF:White,000000:Black,9EA2A2:Grey,008E3D:Green,003083:Blue,F9D500:Yellow,D50032:Red,FEE665:Transparent Yellow,B8D4B7:Translucent Green,E8474E:Transparent Red,FFB246:Translucent Orange,9DCCE1:Translucent Blue,FFD168:Translucent Yellow,916AA7:Transparent Purple,F8B1B1:Translucent Pink,FF9451:Transparent Orange,9FBF52:Transparent Green,589FDC:Transparent Blue,DEB8A5:Translucent Brown', // Hyper PETG
  '03001': 'FFFFFF:White,9EA2A2:Grey,000000:Black,FBDB67:Yellow,DA2C1F:Red,FF6B01:Orange,F4A7D5:Pink,588C44:Green,942A7E:Purple,EED540:Translucent Yellow,7CCFE7:Cloud Blue,783ED2:Deep Violet,1584C1:Ice Blue,FFE101:Bloom Yellow,54BFAF:Ice Cyan,F3871E:Leaf Orange,19909A:Cyan,A892DB:Pale Lavender,F25B85:Strawberry Red,93AF66:Grape Green', // Hyper ABS
  '09002': 'FFFFFF:White,000000:Black,32B801:Green,FF6923:Orange,4AA0DF:Light Blue,AC2629:Fire Red,0352B4:Blue,B3B2B1:Silvery,FB6580:Pink,6A8298:Light Grey,2B597F:Grey,A30369:Purple', // ENDER FAST PLA
  '04001': '555685,2AB192,C6E86C,FFC945,FF9EA3,AE5588,FFFFFF:Ivory White,000000:Black,E10901:Red,FCE303:Yellow,0392D8:Blue,9EA2A2:Grey,03C760:Green,8A8E8C:Silver,F580B5:Peach Pink,C7C9C7:Light Grey,F4CDD4:Light Pink,FF6900:Sunburst Orange,F1E6B2:Caramel Cream,F3EA5D:Lemon Yellow,C0DF16:Tender Sprout Green,00C389:Mint Green,00B5E2:Lake Blue,171C8F:Dawn Blue,611CCC:Morning Glory Purple,BF9BDE:Lavender,FF9DA2:Blush Glow,B8DDE1:Morning Cyan,B2CAE8:Blue Finch,D4EC8E:Matcha Creamshake,91852C:Olive Moss,952C55:Grape Soda', // CR-PLA
  '05001': 'FFC947:Gold,BB5A29:Red Copper,FF8143,AFD101,497C7D,817585,FFFFFF,520B7A:Purple,0A64C9:Blue,B3B5B3:Silver,D6031D,039780,F67798,F9E267', // CR-Silk
  '06001': 'FFFFFF:White,000000:Black,B3B5B3:Grey,2EC94F:Green,7C594A:Brown,E8DBB7:Beige,0B59C2:Blue,FFC703:Yellow,2125AB:Transparent Blue,E10903:Red,796EB0:Purple,FF7702:Orange,E89DAF:Pink,E93CAC:Magenta,D05A57:Transparent Wine Red,68728D:Olive Drab,00B140:Transparent Green', // CR-PETG
  '08001': 'FFFFFF:White,000000:Black,808D9B:Grey,C7372A:Red,0379D0:Blue,F6EB61:Yellow', // Ender-PLA
  '09001': 'FFFFFF:White,000000:Black,FF6923:Orange,7E888F:Grey,C20319:Red,4CCAE1:Blue,7EE0D4:Jade Green,E4D5D4:Beige,7CCD03:Apple Green,4280CA,F28381,03C760,7ECD6C:Green', // EN-PLA+
  '14001': 'FFFFFF:Gypsum White,000000:Black,3A4B69:Navy Blue,95A0AB:Ashes Grey,F65477:Strawberry Red,87D396:Avocado Green,D9EA99,A5DBE8,F5DADD', // CR-PLA Matte
  '15001': 'FF7478:Fluo Red,E9EC6D:Fluo Yellow,FF6C17:Fluo Orange,2EC94F:Fluo Green', // CR-PLA Fluo
  '16001': 'FFFFFF:White,32B801:Green,000000:Black,E10903:Red,036CC7:Blue', // CR-TPU
  '01601': 'D9B8F1:Almond Purple,FD7D36:Rosehip,F16D7A:Strawberry Milk,B8F1CC:Light Green,B2BA81:Greenery,FFE543:Pineapple Yellow,9EA2A2:Grey,B8F1ED:Ocean Blue,212322:Black,EFEFE8:White,FFB3AB:Grapefruit Pink,F8E59A:Butter Yellow,CFCDC9:Mist Grey,FCC89B:Pale Orange,5E93DB:Jeans Blue,E2665C:Sunset Red,F5DADF:Sakura Pink,A9C23F:White Grape,B46B7A:Dusty Rose', // Soleyin Ultra PLA
  '06003': '3D3935:Black,63666A:Night Grey,475D76:Dusk Blue,774212:Caramel Brown Coffee,403845:Phantom Purple,6F5D58:Milk Tea Brown,653024:Danxia Red,32464D:Ancient Wood Green', // Hyper PETG-CF
  '01004': 'A1877F:Coffee,2F6F7A:Green,A0A2B0:Grey,DA8879:Pink,A9533F:Brown,EA733D:Orange,F5E1A4:Yellow,283883:Blue,613E5F:Purple', // Hyper Stardust
  '06004': '8B5B29:Truffle Brown,732E4A:Eggplant Purple,EAB19E:Muddy Pink,A3C7D2:Dusty Blue,E1E000:Chartreuse,A84544:Dusty Red', // Hyper PETG-GF
  '29001': 'D14124:Red,4F758B:Blue,32645C:Green,DBE2E9:White', // Hyper Marble
  '01003': '84BBDE:Blue,ACD1A0:Green,EFEC8A:Yellow,E38090:Pink', // Hyper Luminous
  '06005': '000000:Black,EFEFE8:White,6270B3:Sunset Blue,D5C3D6:Lavender,DD8D5B:Chamomile,B76E6D:Brick Red,F1C501:Ginkgo Yellow,804B68:Mangosteen Purple,C0CD7F:Fern Green,D7C5B6:Milk Tea Brown,9FA7B5:Glacier Grey,95AACA:Snow Mountain Blue', // Soleyin Basic PETG
  '07001': 'FFFFFF:White,E10903:Red,FCE303:Yellow,4F6B96:Grey,000000:Black,0349BC:Blue,F1EB9C:Soft Butter,A4DBE8:Mist Blue,A2E4B8:Bamboo Jade,F58132:Honey Tangerine,74BB7D:Canola Green,CDB5A7:Light Brown,B4B5DF:Smoky Lavender,E0A2D4:Blush Lilac,DB3EB1:Fuchsia,BC8C3D:Soft Mocha', // CR-ABS
}

// colors used for materials without a list of their own (Hyper PLA)
const DEFAULT_ID = '01001'

const parse = (list) =>
  list.split(',').map((entry) => {
    const [hex, name = ''] = entry.split(':')
    return { hex: `#${hex}`, name }
  })

export function officialColors(id) {
  return parse(OFFICIAL[id] ?? OFFICIAL[DEFAULT_ID])
}
